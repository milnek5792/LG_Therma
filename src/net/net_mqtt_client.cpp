// net_mqtt_client.cpp — EMQX TLS přes PubSubClient (ESP32-S3 7B)
#include "net_mqtt_client.h"

#include "mqtt_config.h"
#include "mqtt_emqxsl_ca.h"
#include "net_ntp_time.h"
#include "net_wifi_mgr.h"
#include "climate_ble_room.h"
#include "storage_config_nvs.h"
#include "ui_eez_model.h"
#include "ui_ui_lvgl.h"

#include <Arduino.h>
#include <esp_log.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <lwip/netdb.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

namespace {

static const char* TAG = "MQTT";

class MqttSecureClient : public WiFiClientSecure {
 public:
  IPAddress forcedIp{};
  bool haveForcedIp = false;
  volatile bool assumeUp = false;

  int connect(const char* host, uint16_t port) override {
    assumeUp = false;
    if (haveForcedIp) {
      ESP_LOGI(TAG, "TLS SNI=%s ip=%s", host, forcedIp.toString().c_str());
      return WiFiClientSecure::connect(
          forcedIp, port, host, nullptr, nullptr, nullptr);
    }
    return WiFiClientSecure::connect(host, port);
  }

  int connect(IPAddress ip, uint16_t port) override {
    assumeUp = false;
    return WiFiClientSecure::connect(
        ip, port, MQTT_HOST, nullptr, nullptr, nullptr);
  }

  uint8_t connected() override { return assumeUp ? 1 : 0; }

  void stop() override {
    assumeUp = false;
    WiFiClientSecure::stop();
  }
};

MqttSecureClient s_tls;
PubSubClient s_mqtt(s_tls);

bool s_enabled = false;
bool s_connected = false;
bool s_wantConnect = false;
bool s_busy = false;
bool s_requestConnect = false;
uint32_t s_lastTeleMs = 0;
uint32_t s_lastReconnectMs = 0;
uint32_t s_reconnectBackoffMs = 15000;
uint32_t s_linkUpMs = 0;

char s_status[40] = "Odpojeno";
char s_host[64] = "";

void setStatus(const char* text) {
  strncpy(s_status, text ? text : "", sizeof(s_status) - 1);
  s_status[sizeof(s_status) - 1] = '\0';
}

void fillHostDisplay() {
  snprintf(s_host, sizeof(s_host), "%s:%d", MQTT_HOST, MQTT_PORT);
}

bool timeOkForTls() {
  return time(nullptr) >= 1700000000 && netNtpIsSynced();
}

bool resolveMqttIpv4(IPAddress& out) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  const int err = getaddrinfo(MQTT_HOST, nullptr, &hints, &res);
  if (err != 0 || !res || !res->ai_addr) {
    ESP_LOGI(TAG, "DNS IPv4 selhalo (%d)", err);
    if (res) {
      freeaddrinfo(res);
    }
    return false;
  }
  const auto* addr4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
  out = IPAddress(addr4->sin_addr.s_addr);
  freeaddrinfo(res);
  return true;
}

void publishFloat(const char* topic, float v) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", (double)v);
  s_mqtt.publish(topic, buf, true);  // retain
}

void publishStr(const char* topic, const char* v) {
  s_mqtt.publish(topic, v, true);  // retain
}

// Connect sync (stagger) + pak jen při změně. Žádný minutový cyklus.
int s_teleIdx = -1;
uint32_t s_teleStartMs = 0;
uint32_t s_lastChangeCheckMs = 0;

struct TeleSnap {
  float outlet = -999.0f;
  float setp = -999.0f;
  float room = -999.0f;
  int8_t power = -1;
  int8_t compressor = -1;
  int8_t alarm = -1;
};

TeleSnap s_pub{};

bool nearlyEq(float a, float b) {
  return fabsf(a - b) < 0.05f;
}

void publishOutlet(float v) {
  publishFloat(MQTT_TOPIC_TELE_TEMP_OUTLET, v);
  s_pub.outlet = v;
  ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_OUTLET, (double)v);
}

void publishTeleOne(int idx) {
  switch (idx) {
#if MQTT_TELE_OUTLET_DEMO
    case 0:
      publishOutlet(32.0f);
      break;
    case 1:
      publishOutlet(35.5f);
      break;
    case 2:
      publishOutlet(41.0f);
      break;
    case 3:
      publishFloat(MQTT_TOPIC_TELE_TEMP_SET, uiEez.teplota_vody_set);
      s_pub.setp = uiEez.teplota_vody_set;
      break;
    case 4:
      publishFloat(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni);
      s_pub.room = uiEez.teplota_vnitrni;
      break;
    case 5:
      publishStr(MQTT_TOPIC_TELE_POWER, uiEez.sig_chod ? "ON" : "OFF");
      s_pub.power = uiEez.sig_chod ? 1 : 0;
      break;
    case 6:
      publishStr(MQTT_TOPIC_TELE_COMPRESSOR,
                 uiEez.sig_kompresor ? "ON" : "OFF");
      s_pub.compressor = uiEez.sig_kompresor ? 1 : 0;
      break;
    case 7:
      publishStr(MQTT_TOPIC_TELE_ALARM, uiEez.sig_alarm ? "ON" : "OFF");
      s_pub.alarm = uiEez.sig_alarm ? 1 : 0;
      break;
#else
    case 0:
      publishOutlet(uiEez.teplota_vody_vystup);
      break;
    case 1:
      publishFloat(MQTT_TOPIC_TELE_TEMP_SET, uiEez.teplota_vody_set);
      s_pub.setp = uiEez.teplota_vody_set;
      break;
    case 2:
      publishFloat(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni);
      s_pub.room = uiEez.teplota_vnitrni;
      break;
    case 3:
      publishStr(MQTT_TOPIC_TELE_POWER, uiEez.sig_chod ? "ON" : "OFF");
      s_pub.power = uiEez.sig_chod ? 1 : 0;
      break;
    case 4:
      publishStr(MQTT_TOPIC_TELE_COMPRESSOR,
                 uiEez.sig_kompresor ? "ON" : "OFF");
      s_pub.compressor = uiEez.sig_kompresor ? 1 : 0;
      break;
    case 5:
      publishStr(MQTT_TOPIC_TELE_ALARM, uiEez.sig_alarm ? "ON" : "OFF");
      s_pub.alarm = uiEez.sig_alarm ? 1 : 0;
      break;
#endif
    default:
      break;
  }
}

#if MQTT_TELE_OUTLET_DEMO
static const int kTeleSyncCount = 8;  // 3× outlet demo + 5 stavů
#else
static const int kTeleSyncCount = 6;
#endif

void teleTick() {
  if (!s_connected || s_teleIdx < 0) {
    return;
  }
  static uint32_t s_lastStepMs = 0;
  const uint32_t now = millis();
  if (s_teleIdx > 0 && (now - s_lastStepMs) < MQTT_TELE_STEP_MS) {
    return;
  }
  s_lastStepMs = now;

  publishTeleOne(s_teleIdx);
  ++s_teleIdx;
  if (s_teleIdx >= kTeleSyncCount) {
    ESP_LOGI(TAG, "tele sync END dt=%lu ms (retain)",
             (unsigned long)(millis() - s_teleStartMs));
    s_teleIdx = -1;
    uiLvglSetRgbLowBandwidth(false);
  }
}

void startTeleStaggered() {
  if (!s_connected || s_teleIdx >= 0) {
    return;
  }
#if !MQTT_TELE_ENABLE
  return;
#endif
  s_teleIdx = 0;
  s_teleStartMs = millis();
  uiLvglSetRgbLowBandwidth(true);
  ESP_LOGI(TAG, "tele sync BEGIN (retain) steps=%d", kTeleSyncCount);
  teleTick();
}

void telePublishChanges() {
  if (!s_connected || s_teleIdx >= 0) {
    return;
  }
  const uint32_t now = millis();
  if (now - s_lastChangeCheckMs < 500) {
    return;
  }
  s_lastChangeCheckMs = now;

  const float outlet = uiEez.teplota_vody_vystup;
#if MQTT_TELE_OUTLET_DEMO
  // Demo: nech retained test (32→35.5→41), nepřebíjej UI hodnotou
  (void)outlet;
#else
  if (!nearlyEq(outlet, s_pub.outlet)) {
    uiLvglSetRgbLowBandwidth(true);
    publishOutlet(outlet);
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
#endif
  if (!nearlyEq(uiEez.teplota_vody_set, s_pub.setp)) {
    uiLvglSetRgbLowBandwidth(true);
    publishFloat(MQTT_TOPIC_TELE_TEMP_SET, uiEez.teplota_vody_set);
    s_pub.setp = uiEez.teplota_vody_set;
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  if (!nearlyEq(uiEez.teplota_vnitrni, s_pub.room)) {
    if (uiEez.teplota_vnitrni > UI_TEPLOTA_NEPLATNA + 100.0f) {
      uiLvglSetRgbLowBandwidth(true);
      publishFloat(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni);
      s_pub.room = uiEez.teplota_vnitrni;
      uiLvglSetRgbLowBandwidth(false);
      return;
    }
    s_pub.room = uiEez.teplota_vnitrni;
  }
  const int8_t pow = uiEez.sig_chod ? 1 : 0;
  if (pow != s_pub.power) {
    uiLvglSetRgbLowBandwidth(true);
    publishStr(MQTT_TOPIC_TELE_POWER, pow ? "ON" : "OFF");
    s_pub.power = pow;
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  const int8_t comp = uiEez.sig_kompresor ? 1 : 0;
  if (comp != s_pub.compressor) {
    uiLvglSetRgbLowBandwidth(true);
    publishStr(MQTT_TOPIC_TELE_COMPRESSOR, comp ? "ON" : "OFF");
    s_pub.compressor = comp;
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  const int8_t al = uiEez.sig_alarm ? 1 : 0;
  if (al != s_pub.alarm) {
    uiLvglSetRgbLowBandwidth(true);
    publishStr(MQTT_TOPIC_TELE_ALARM, al ? "ON" : "OFF");
    s_pub.alarm = al;
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
}

bool topicIs(const char* topic, unsigned int len, const char* expect) {
  const size_t el = strlen(expect);
  return len == el && strncmp(topic, expect, el) == 0;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[64];
  const unsigned int n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = '\0';
  ESP_LOGI(TAG, "CMD %s = %s", topic, msg);

  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_SETPOINT)) {
    const float sp = atof(msg);
    if (sp >= 15.0f && sp <= 65.0f) {
      uiEez.teplota_vody_set = sp;
    }
    return;
  }
  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_MODE)) {
    if (strcasecmp(msg, "auto") == 0) {
      uiEez.rezim = UI_REZIM_AUTO;
    } else {
      uiEez.rezim = UI_REZIM_VYSTUPNI_TEPLOTA;
    }
    return;
  }
  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_QUIET)) {
    uiEez.sig_utlum = (strcasecmp(msg, "ON") == 0 || strcmp(msg, "1") == 0);
    return;
  }
  // power / watch — bez LIN na 7B zatím jen log
}

bool subscribeAll() {
  const char* topics[] = {
      MQTT_TOPIC_CMD_WATCH,
      MQTT_TOPIC_CMD_POWER,
      MQTT_TOPIC_CMD_SETPOINT,
      MQTT_TOPIC_CMD_MODE,
      MQTT_TOPIC_CMD_QUIET,
  };
  for (size_t i = 0; i < sizeof(topics) / sizeof(topics[0]); ++i) {
    if (!s_mqtt.subscribe(topics[i])) {
      ESP_LOGI(TAG, "SUB fail %s", topics[i]);
      return false;
    }
    ESP_LOGI(TAG, "SUB ok %s", topics[i]);
    delay(50);
    s_mqtt.loop();
  }
  return true;
}

bool reconnectOnce() {
  fillHostDisplay();
  if (!timeOkForTls()) {
    setStatus("Cekam cas/NTP");
    return false;
  }
  if (!netWifiIsConnected()) {
    setStatus("Cekam WiFi...");
    return false;
  }
  if (climateBleIsBusy()) {
    setStatus("Cekam BLE...");
    ESP_LOGI(TAG, "TLS odlozen — BLE busy");
    return false;
  }

  s_busy = true;
  ESP_LOGI(TAG, "freeze ON (TLS) ms=%lu", (unsigned long)millis());
  uiLvglSetFrozen(true);
  setStatus("Pripojovani...");

  IPAddress ip;
  if (!resolveMqttIpv4(ip)) {
    setStatus("DNS selhalo");
    ESP_LOGI(TAG, "freeze OFF (DNS fail) ms=%lu", (unsigned long)millis());
    uiLvglSetFrozen(false);
    s_busy = false;
    return false;
  }
  ESP_LOGI(TAG, "DNS IPv4 %s -> %s", MQTT_HOST, ip.toString().c_str());
  s_tls.forcedIp = ip;
  s_tls.haveForcedIp = true;

  s_tls.assumeUp = false;
  s_mqtt.disconnect();
  s_tls.stop();
  delay(200);

  static const char* kMqttAlpn[] = {"mqtt", nullptr};
  s_tls.setAlpnProtocols(kMqttAlpn);
#if MQTT_TLS_INSECURE
  s_tls.setInsecure();
  ESP_LOGI(TAG, "TLS insecure (debug)");
#else
  s_tls.setCACert(MQTT_EMQXSL_CA_ROOT);
#endif
  s_tls.setHandshakeTimeout(30);
  s_tls.setTimeout(1000);

  s_mqtt.setServer(MQTT_HOST, (uint16_t)MQTT_PORT);
  s_mqtt.setCallback(mqttCallback);
  s_mqtt.setBufferSize(1024);
  s_mqtt.setKeepAlive(60);
  s_mqtt.setSocketTimeout(20);

  ESP_LOGI(TAG, "connect %s as %s...", s_host, MQTT_CLIENT_ID);
  ESP_LOGI(TAG,
           "heap free=%u maxblk=%u dma_max=%u psram=%u",
           (unsigned)ESP.getFreeHeap(),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
           (unsigned)ESP.getFreePsram());

  const uint32_t t0 = millis();
  const bool ok = s_mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  ESP_LOGI(TAG, "connect %s (%lu ms) state=%d",
           ok ? "OK" : "FAIL",
           (unsigned long)(millis() - t0),
           s_mqtt.state());

  if (!ok) {
    setStatus(s_mqtt.state() == -4 ? "Timeout" : "MQTT fail");
    s_tls.assumeUp = false;
    s_tls.stop();
    s_connected = false;
    delay(500);
    ESP_LOGI(TAG, "freeze OFF (fail) ms=%lu", (unsigned long)millis());
    uiLvglSetFrozen(false);
    s_busy = false;
    return false;
  }

  s_tls.assumeUp = true;
  s_connected = true;
  ESP_LOGI(TAG, "TLS+MQTT session up, subscribe...");

  if (!subscribeAll()) {
    s_tls.assumeUp = false;
    s_mqtt.disconnect();
    s_tls.stop();
    s_connected = false;
    setStatus("SUB fail");
    delay(500);
    ESP_LOGI(TAG, "freeze OFF (sub fail) ms=%lu", (unsigned long)millis());
    uiLvglSetFrozen(false);
    s_busy = false;
    return false;
  }

  ESP_LOGI(TAG, "publish availability=online");
  s_mqtt.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
#if MQTT_TELE_ENABLE
  // Nesypej 15 topiců najednou — RF spike i při LVGL freeze (panel pořád skenuje)
  startTeleStaggered();
#else
  ESP_LOGI(TAG, "tele DISABLED (MQTT_TELE_ENABLE=0) — RGB diag");
#endif
  for (int i = 0; i < 10; ++i) {
    s_mqtt.loop();
    delay(50);
  }

  s_linkUpMs = millis();
  s_lastTeleMs = millis();
  s_reconnectBackoffMs = 15000;
  setStatus("Pripojeno");
  ESP_LOGI(TAG, "pripojeno ms=%lu — unfreeze za 800ms",
           (unsigned long)s_linkUpMs);

  delay(800);
  ESP_LOGI(TAG, "freeze OFF (ok) ms=%lu flush_hint=ui",
           (unsigned long)millis());
  uiLvglSetFrozen(false);
  s_busy = false;
  return true;
}

void disconnectMqtt() {
  ESP_LOGI(TAG, "disconnect BEGIN connected=%d ms=%lu",
           (int)s_connected, (unsigned long)millis());
  if (s_connected) {
    s_mqtt.publish(MQTT_TOPIC_AVAILABILITY, "offline", true);
    s_mqtt.loop();
  }
  s_tls.assumeUp = false;
  s_mqtt.disconnect();
  s_tls.stop();
  s_connected = false;
  setStatus(s_enabled ? "Odpojeno" : "Vypnuto");
  ESP_LOGI(TAG, "disconnect END ms=%lu", (unsigned long)millis());
}

}  // namespace

void netMqttInit() {
  fillHostDisplay();
  s_enabled = storageLoadMqttEnabled();
  s_wantConnect = s_enabled;
  setStatus(s_enabled ? "Pripraveno" : "Vypnuto");
  ESP_LOGI(TAG, "init enabled=%d host=%s", (int)s_enabled, s_host);
}

void netMqttSetEnabled(bool on) {
  ESP_LOGI(TAG, "setEnabled %d -> %d", (int)s_enabled, (int)on);
  s_enabled = on;
  storageSaveMqttEnabled(on);
  if (!on) {
    s_wantConnect = false;
    s_requestConnect = false;
    disconnectMqtt();
    setStatus("Vypnuto");
    return;
  }
  s_wantConnect = true;
  setStatus("Pripraveno");
}

bool netMqttIsEnabled() { return s_enabled; }

void netMqttConnect() {
  ESP_LOGI(TAG, "Connect requested wifi=%d ntp=%d ms=%lu",
           (int)netWifiIsConnected(), (int)netNtpIsSynced(),
           (unsigned long)millis());
  if (!s_enabled) {
    netMqttSetEnabled(true);
  }
  s_wantConnect = true;
  s_requestConnect = true;
  setStatus("Pripojovani...");
}

bool netMqttIsConnected() { return s_connected; }
bool netMqttIsBusy() { return s_busy || s_teleIdx >= 0; }

void netMqttDisconnectQuiet(void) {
  s_teleIdx = -1;
  if (s_connected || s_busy) {
    // Bez availability publish — hned čistý disconnect před BLE
    s_tls.assumeUp = false;
    s_mqtt.disconnect();
    s_tls.stop();
    s_connected = false;
    s_busy = false;
    setStatus(s_enabled ? "Odpojeno" : "Vypnuto");
    ESP_LOGI(TAG, "disconnectQuiet (pre-BLE)");
  }
}

const char* netMqttStatus() { return s_status; }
const char* netMqttHost() { return s_host; }

void netMqttTick() {
  if (!s_enabled) {
    return;
  }

  static uint32_t s_lastHbMs = 0;
  const uint32_t now = millis();

  if (s_requestConnect) {
    ESP_LOGI(TAG, "tick: requestConnect wifi=%d ntp=%d",
             (int)netWifiIsConnected(), (int)netNtpIsSynced());
    s_requestConnect = false;
    if (!netWifiIsConnected()) {
      setStatus("Nejdriv Wi-Fi");
      ESP_LOGI(TAG, "abort: no Wi-Fi");
      return;
    }
    if (!timeOkForTls()) {
      setStatus("Cekam cas/NTP");
      ESP_LOGI(TAG, "wait NTP, retry");
      s_requestConnect = true;
      return;
    }
    ESP_LOGI(TAG, "tick: calling reconnectOnce()");
    reconnectOnce();
    s_lastReconnectMs = millis();
    return;
  }

  if (!s_wantConnect || !netWifiIsConnected()) {
    if (s_connected) {
      ESP_LOGI(TAG, "tick: wifi/want lost -> disconnect");
      disconnectMqtt();
    }
    return;
  }

  if (s_connected) {
    // mqtt.loop méně často — TLS keepalive žere PSRAM bandwidth (RGB underrun)
    static uint32_t s_lastMqttLoopMs = 0;
    if (s_teleIdx < 0 && (now - s_lastMqttLoopMs) >= 500) {
      s_lastMqttLoopMs = now;
      const uint32_t tLoop0 = millis();
      s_mqtt.loop();
      const uint32_t loopDt = millis() - tLoop0;
      if (loopDt > 20) {
        ESP_LOGI(TAG, "loop SLOW dt=%lu ms state=%d",
                 (unsigned long)loopDt, s_mqtt.state());
      }
    } else if (s_teleIdx >= 0) {
      // během tele nevolat loop každý tick
    }

    if (!s_mqtt.connected() || !s_tls.assumeUp) {
      if (millis() - s_linkUpMs > 10000) {
        ESP_LOGI(TAG, "session padla state=%d assume=%d",
                 s_mqtt.state(), (int)s_tls.assumeUp);
        disconnectMqtt();
        s_lastReconnectMs = millis();
      }
      return;
    }
    if (s_teleIdx >= 0) {
      teleTick();
    } else if (MQTT_TELE_ENABLE) {
      telePublishChanges();
    }
    if (now - s_lastHbMs >= 5000) {
      s_lastHbMs = now;
      ESP_LOGI(TAG, "HB up=%lus tele=%s heap=%u flush=%u",
               (unsigned long)((now - s_linkUpMs) / 1000),
               MQTT_TELE_ENABLE ? "retain/chg" : "OFF",
               (unsigned)ESP.getFreeHeap(),
               (unsigned)uiLvglFlushCount());
    }
    setStatus("Pripojeno");
    return;
  }

  if (timeOkForTls() &&
      (millis() - s_lastReconnectMs) >= s_reconnectBackoffMs) {
    s_lastReconnectMs = millis();
    ESP_LOGI(TAG, "auto-reconnect backoff=%lu ms",
             (unsigned long)s_reconnectBackoffMs);
    if (!reconnectOnce()) {
      if (s_reconnectBackoffMs < 60000) {
        s_reconnectBackoffMs += 15000;
      }
    } else {
      s_reconnectBackoffMs = 15000;
    }
  }
}
