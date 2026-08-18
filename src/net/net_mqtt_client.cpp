// net_mqtt_client.cpp — EMQX TLS přes PubSubClient (ESP32-S3 7B)
#include "net_mqtt_client.h"

#include "mqtt_config.h"
#include "mqtt_emqxsl_ca.h"
#include "net_ntp_time.h"
#include "net_wifi_mgr.h"
#include "climate_ble_room.h"
#include "storage_config_nvs.h"
#include "bus_lg_config.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "ui_bus_bindings.h"
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

bool mqttPublishRetain(const char* topic, const char* v) {
  if (!topic || !v) {
    return false;
  }
  bool ok = s_mqtt.publish(topic, v, true);
  s_mqtt.loop();  // nutný flush — bez toho pozdní tele (compressor) často neodejde
  if (!ok) {
    delay(20);
    s_mqtt.loop();
    ok = s_mqtt.publish(topic, v, true);
    s_mqtt.loop();
    ESP_LOGI(TAG, "PUB retry %s -> %d", topic, (int)ok);
  }
  return ok;
}

void publishFloat(const char* topic, float v) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", (double)v);
  mqttPublishRetain(topic, buf);
}

void publishStr(const char* topic, const char* v) {
  mqttPublishRetain(topic, v);
}

bool tempOffline(float v) {
  return v <= UI_TEPLOTA_NEPLATNA + 100.0f;
}

/** Platná T → číslo (retain). Neplatná → nepublikuj (nech poslední retain). */
bool publishTemp(const char* topic, float v) {
  if (tempOffline(v)) {
    return false;
  }
  publishFloat(topic, v);
  return true;
}

/** LIN teploty přímo z modelu (nečekej na UI tick — core0 vs core1). */
float linTempOrNa(uint8_t raw, bool maA0) {
  if (!maA0 || raw == 0 || raw >= 100) {
    return UI_TEPLOTA_NEPLATNA;
  }
  return (float)raw;
}

void snapLinTemps(float* inlet, float* outlet, float* outdoor, float* setp) {
  lgModelLock();
  const bool maA0 = lgMaCerstoA0();
  const uint8_t in = mVstupni;
  const uint8_t out = mVystupni;
  const uint8_t ven = lgModelA0Bajt(5);
  const uint8_t cil = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  lgModelUnlock();

  if (inlet) {
    *inlet = linTempOrNa(in, maA0);
  }
  if (outlet) {
    *outlet = linTempOrNa(out, maA0);
  }
  if (outdoor) {
    *outdoor = linTempOrNa(ven, maA0 && ven < 80);
  }
  if (setp) {
    if (cil >= 15 && cil <= 65) {
      *setp = (float)cil;  // live nebo last-known
    } else {
      *setp = UI_TEPLOTA_NEPLATNA;
    }
  }
}

// Connect sync (stagger) + pak jen při změně. Žádný minutový cyklus.
int s_teleIdx = -1;
uint32_t s_teleStartMs = 0;
uint32_t s_lastChangeCheckMs = 0;

struct TeleSnap {
  float outlet = -999.0f;
  float inlet = -999.0f;
  float outdoor = -999.0f;
  float setp = -999.0f;
  float room = -999.0f;
  int8_t power = -1;
  int8_t pump = -1;
  int8_t compressor = -1;
  int8_t lin = -1;  // A0 čerstvé = Tc připojeno na LIN
  int8_t alarm = -1;
};

TeleSnap s_pub{};
int8_t s_watchPub = -1;  // poslední tele/watch
/** cmd/compressor ON = test; nebo MQTT_COMPRESSOR_FORCE_ON=1 = vždy ON */
bool s_compTest = (MQTT_COMPRESSOR_FORCE_ON != 0);
uint32_t s_compLastPubMs = 0;

/** Watch OFF / disconnect: vše OFF/___ — panel jasně „odpojeno“. */
void publishTeleOfflineMarkers() {
  if (!s_connected) {
    return;
  }
  uiLvglSetRgbLowBandwidth(true);
  mqttPublishRetain(MQTT_TOPIC_TELE_WATCH, "OFF");
  s_watchPub = 0;
  mqttPublishRetain(MQTT_TOPIC_TELE_LIN, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_POWER, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_PUMP, "OFF");
  // Test kompresoru / FORCE_ON přežije watch OFF
  if (s_compTest || MQTT_COMPRESSOR_FORCE_ON) {
    mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, "ON");
    s_pub.compressor = 1;
  } else {
    mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, "OFF");
    s_pub.compressor = 0;
  }
  mqttPublishRetain(MQTT_TOPIC_TELE_ALARM, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_OUTLET, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_INLET, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_OUTDOOR, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_ROOM, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_SET, MQTT_TELE_NA);
  const int8_t keepComp = s_pub.compressor;
  s_pub = TeleSnap{};
  s_pub.lin = 0;
  s_pub.power = 0;
  s_pub.pump = 0;
  s_pub.compressor = keepComp;
  s_pub.alarm = 0;
  uiLvglSetRgbLowBandwidth(false);
  ESP_LOGI(TAG, "tele → OFF/___ (watch off) compTest=%d", (int)s_compTest);
}

#if MQTT_TELE_REQUIRE_WATCH
bool s_watchOn = false;
uint32_t s_watchUntilMs = 0;

bool watchActive() {
  if (!s_watchOn) {
    return false;
  }
  return (int32_t)(s_watchUntilMs - millis()) > 0;
}

void publishWatchState(bool on) {
  if (!s_connected) {
    return;
  }
  const int8_t v = on ? 1 : 0;
  if (s_watchPub == v) {
    return;
  }
  s_watchPub = v;
  mqttPublishRetain(MQTT_TOPIC_TELE_WATCH, on ? "ON" : "OFF");
  ESP_LOGI(TAG, "retain %s = %s", MQTT_TOPIC_TELE_WATCH, on ? "ON" : "OFF");
}

void bumpWatch(uint32_t holdMs = MQTT_WATCH_IDLE_MS) {
  const bool was = watchActive();
  s_watchOn = true;
  const uint32_t until = millis() + holdMs;
  if ((int32_t)(until - s_watchUntilMs) > 0) {
    s_watchUntilMs = until;
  }
  ESP_LOGI(TAG, "watch ON %lu s (was=%d)", (unsigned long)(holdMs / 1000),
           (int)was);
  publishWatchState(true);
}

void watchOff(bool clearTopics = true) {
  const bool was = s_watchOn || s_teleIdx >= 0 || s_watchPub == 1;
  s_watchOn = false;
  s_watchUntilMs = 0;
  s_teleIdx = -1;
  uiLvglSetRgbLowBandwidth(false);
  if (clearTopics && was) {
    // lin + celá telemetrie OFF/___ — i když A0 ještě žije
    publishTeleOfflineMarkers();
  } else {
    publishWatchState(false);
  }
  ESP_LOGI(TAG, "watch OFF — tele cleared=%d", (int)clearTopics);
}

bool teleAllowed() {
#if MQTT_TELE_ENABLE
  return watchActive();
#else
  return false;
#endif
}
#else
bool teleAllowed() {
  return MQTT_TELE_ENABLE != 0;
}
void bumpWatch(uint32_t /*holdMs*/) {}
void watchOff() {}
#endif

bool nearlyEq(float a, float b) {
  return fabsf(a - b) < 0.05f;
}

void publishOutlet(float v) {
  if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTLET, v)) {
    s_pub.outlet = v;
    ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_OUTLET, (double)v);
  } else {
    // Neplatná — nech starý retain i s_pub (ať se znovu zkusí po obnově)
    ESP_LOGI(TAG, "temp_outlet skip (NA)");
  }
}

/** Setpoint: vždy číslo (%.1f) pro slider; bez hodnoty nech retain. */
void publishSetpoint() {
  float setp = UI_TEPLOTA_NEPLATNA;
  snapLinTemps(nullptr, nullptr, nullptr, &setp);
  if (!tempOffline(setp)) {
    publishFloat(MQTT_TOPIC_TELE_TEMP_SET, setp);
    s_pub.setp = setp;
    ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_SET, (double)setp);
  } else {
    s_pub.setp = UI_TEPLOTA_NEPLATNA;
    ESP_LOGI(TAG, "temp_set skip (neni platna cilova)");
  }
}

bool snapCompressorOn(uint8_t* b2Out = nullptr, uint8_t* b3Out = nullptr) {
  lgModelLock();
  const bool maA0 = lgMaCerstoA0();
  const uint8_t b2 = lgModelA0Bajt(2);
  const uint8_t b3 = lgModelA0Bajt(3);
  lgModelUnlock();
  if (b2Out) {
    *b2Out = b2;
  }
  if (b3Out) {
    *b3Out = b3;
  }
  if (s_compTest || MQTT_COMPRESSOR_FORCE_ON) {
    return true;
  }
  // Stejně jako LED na 7B
  return uiEez.sig_kompresor || (maA0 && lgJeKompresorBezi(b3));
}

void publishCompressor(bool on) {
  uint8_t b2 = 0, b3 = 0;
  lgModelLock();
  const bool maA0 = lgMaCerstoA0();
  b2 = lgModelA0Bajt(2);
  b3 = lgModelA0Bajt(3);
  lgModelUnlock();
#if MQTT_COMPRESSOR_FORCE_ON
  on = true;
  s_compTest = true;
#else
  if (s_compTest) {
    on = true;
  }
#endif
  const char* v = on ? "ON" : "OFF";
  const bool ok = mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, v);
  s_pub.compressor = on ? 1 : 0;
  s_compLastPubMs = millis();
  ESP_LOGI(TAG,
           "retain %s = %s ok=%d force=%d ma=%d ui=%d B2=%02X B3=%02X",
           MQTT_TOPIC_TELE_COMPRESSOR, v, (int)ok, (int)MQTT_COMPRESSOR_FORCE_ON,
           (int)maA0, (int)uiEez.sig_kompresor, b2, b3);
}

void publishTeleOne(int idx) {
  float inlet = UI_TEPLOTA_NEPLATNA;
  float outlet = UI_TEPLOTA_NEPLATNA;
  float outdoor = UI_TEPLOTA_NEPLATNA;
  float setp = UI_TEPLOTA_NEPLATNA;

  switch (idx) {
    // Binární první — compressor nesmí spadnout na konec fronty
    case 0: {
      const bool linOk = lgMaCerstoA0();
      publishStr(MQTT_TOPIC_TELE_LIN, linOk ? "ON" : "OFF");
      s_pub.lin = linOk ? 1 : 0;
      ESP_LOGI(TAG, "retain %s = %s", MQTT_TOPIC_TELE_LIN, linOk ? "ON" : "OFF");
      break;
    }
    case 1:
      publishStr(MQTT_TOPIC_TELE_POWER, uiEez.sig_chod ? "ON" : "OFF");
      s_pub.power = uiEez.sig_chod ? 1 : 0;
      break;
    case 2:
      publishStr(MQTT_TOPIC_TELE_PUMP, uiEez.sig_cerpadlo ? "ON" : "OFF");
      s_pub.pump = uiEez.sig_cerpadlo ? 1 : 0;
      ESP_LOGI(TAG, "retain %s = %s", MQTT_TOPIC_TELE_PUMP,
               uiEez.sig_cerpadlo ? "ON" : "OFF");
      break;
    case 3: {
      uint8_t b2 = 0, b3 = 0;
      const bool on = snapCompressorOn(&b2, &b3);
      publishCompressor(on);
      break;
    }
    case 4:
      snapLinTemps(&inlet, &outlet, &outdoor, &setp);
      publishOutlet(outlet);
      break;
    case 5:
      snapLinTemps(&inlet, &outlet, &outdoor, &setp);
      if (publishTemp(MQTT_TOPIC_TELE_TEMP_INLET, inlet)) {
        s_pub.inlet = inlet;
        ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_INLET,
                 (double)inlet);
      }
      break;
    case 6:
      snapLinTemps(&inlet, &outlet, &outdoor, &setp);
      if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTDOOR, outdoor)) {
        s_pub.outdoor = outdoor;
        ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_OUTDOOR,
                 (double)outdoor);
      }
      break;
    case 7:
      publishSetpoint();
      break;
    case 8:
      if (publishTemp(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni)) {
        s_pub.room = uiEez.teplota_vnitrni;
        ESP_LOGI(TAG, "retain %s = %.1f", MQTT_TOPIC_TELE_TEMP_ROOM,
                 (double)uiEez.teplota_vnitrni);
      }
      break;
    case 9:
      publishStr(MQTT_TOPIC_TELE_ALARM, uiEez.sig_alarm ? "ON" : "OFF");
      s_pub.alarm = uiEez.sig_alarm ? 1 : 0;
      break;
    default:
      break;
  }
}

static const int kTeleSyncCount = 10;
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
  if (!teleAllowed()) {
    return;
  }
  s_pub = TeleSnap{};  // vynutit plný sync
  s_teleIdx = 0;
  s_teleStartMs = millis();
  uiLvglSetRgbLowBandwidth(true);
  ESP_LOGI(TAG, "tele sync BEGIN (watch) steps=%d", kTeleSyncCount);
  teleTick();
}

void telePublishChanges() {
  if (!s_connected || s_teleIdx >= 0 || !teleAllowed()) {
    return;
  }
  const uint32_t now = millis();
  if (now - s_lastChangeCheckMs < MQTT_TELE_CHANGE_MS) {
    return;
  }
  s_lastChangeCheckMs = now;

  // LIN live jen při aktivním watch (jinak už je OFF z watchOff)
  uint8_t b2 = 0, b3 = 0;
  const int8_t lin = lgMaCerstoA0() ? 1 : 0;
  const int8_t pow = uiEez.sig_chod ? 1 : 0;
  const int8_t pump = uiEez.sig_cerpadlo ? 1 : 0;
  const int8_t comp = snapCompressorOn(&b2, &b3) ? 1 : 0;
  const int8_t al = uiEez.sig_alarm ? 1 : 0;
  bool binChanged = false;

  // Stejně jako pump: při změně + lehký refresh compressor každých 3 s
  const bool compRefresh = (now - s_compLastPubMs) >= 3000;

  if (lin != s_pub.lin || pow != s_pub.power || pump != s_pub.pump ||
      comp != s_pub.compressor || al != s_pub.alarm || compRefresh) {
    uiLvglSetRgbLowBandwidth(true);
    if (lin != s_pub.lin) {
      publishStr(MQTT_TOPIC_TELE_LIN, lin ? "ON" : "OFF");
      s_pub.lin = lin;
      binChanged = true;
    }
    if (pow != s_pub.power) {
      publishStr(MQTT_TOPIC_TELE_POWER, pow ? "ON" : "OFF");
      s_pub.power = pow;
      binChanged = true;
    }
    if (pump != s_pub.pump) {
      publishStr(MQTT_TOPIC_TELE_PUMP, pump ? "ON" : "OFF");
      s_pub.pump = pump;
      ESP_LOGI(TAG, "retain %s = %s", MQTT_TOPIC_TELE_PUMP, pump ? "ON" : "OFF");
      binChanged = true;
    }
    if (comp != s_pub.compressor || compRefresh) {
      publishCompressor(comp != 0);
      binChanged = true;
    }
    if (al != s_pub.alarm) {
      publishStr(MQTT_TOPIC_TELE_ALARM, al ? "ON" : "OFF");
      s_pub.alarm = al;
      binChanged = true;
    }
    uiLvglSetRgbLowBandwidth(false);
    if (binChanged) {
      return;
    }
  }

  float inlet = UI_TEPLOTA_NEPLATNA;
  float outlet = UI_TEPLOTA_NEPLATNA;
  float outdoor = UI_TEPLOTA_NEPLATNA;
  float setp = UI_TEPLOTA_NEPLATNA;
  snapLinTemps(&inlet, &outlet, &outdoor, &setp);

  // Teploty: max 1 změna / tick (retain jen platná čísla)
  if (!tempOffline(outlet) && !nearlyEq(outlet, s_pub.outlet)) {
    uiLvglSetRgbLowBandwidth(true);
    publishOutlet(outlet);
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  if (!tempOffline(inlet) && !nearlyEq(inlet, s_pub.inlet)) {
    uiLvglSetRgbLowBandwidth(true);
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_INLET, inlet)) {
      s_pub.inlet = inlet;
    }
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  if (!tempOffline(outdoor) && !nearlyEq(outdoor, s_pub.outdoor)) {
    uiLvglSetRgbLowBandwidth(true);
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTDOOR, outdoor)) {
      s_pub.outdoor = outdoor;
    }
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  if (!tempOffline(setp) && !nearlyEq(setp, s_pub.setp)) {
    uiLvglSetRgbLowBandwidth(true);
    publishSetpoint();
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
  if (!tempOffline(uiEez.teplota_vnitrni) &&
      !nearlyEq(uiEez.teplota_vnitrni, s_pub.room)) {
    uiLvglSetRgbLowBandwidth(true);
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni)) {
      s_pub.room = uiEez.teplota_vnitrni;
    }
    uiLvglSetRgbLowBandwidth(false);
    return;
  }
}

bool topicIs(const char* topic, unsigned int len, const char* expect) {
  const size_t el = strlen(expect);
  return len == el && strncmp(topic, expect, el) == 0;
}

void trimInPlace(char* s) {
  if (!s) {
    return;
  }
  char* p = s;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
    ++p;
  }
  if (p != s) {
    memmove(s, p, strlen(p) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 &&
         (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' ||
          s[n - 1] == '\n')) {
    s[--n] = '\0';
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[64];
  const unsigned int n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = '\0';
  trimInPlace(msg);
  ESP_LOGI(TAG, "CMD %s = [%s]", topic, msg);

  auto parseOnOff = [](const char* m, bool* onOut) -> bool {
    if (!m || !onOut) {
      return false;
    }
    if (strcasecmp(m, "ON") == 0 || strcmp(m, "1") == 0 ||
        strcasecmp(m, "true") == 0 || strcasecmp(m, "start") == 0) {
      *onOut = true;
      return true;
    }
    if (strcasecmp(m, "OFF") == 0 || strcmp(m, "0") == 0 ||
        strcasecmp(m, "false") == 0 || strcasecmp(m, "stop") == 0) {
      *onOut = false;
      return true;
    }
    return false;
  };

  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_WATCH)) {
    bool on = false;
    // prázdný payload / "watch" = ON (některé panely tak posílají)
    if (msg[0] == '\0' || strcasecmp(msg, "watch") == 0) {
      on = true;
    } else if (!parseOnOff(msg, &on)) {
      ESP_LOGI(TAG, "cmd/watch neznamy payload [%s]", msg);
      return;
    }
    if (on) {
      const bool was = teleAllowed();
      bumpWatch();
      if (!was) {
        startTeleStaggered();  // první sync po otevření appky
      }
    } else {
      watchOff();
    }
    return;
  }

  // Jakýkoli jiný cmd = někdo ovládá → prodloužit watch
  bumpWatch();

  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_SETPOINT)) {
    // Absolutní / relativní — fronta na loop (LIN na core 1)
    if (strcasecmp(msg, "up") == 0 || strcmp(msg, "+") == 0) {
      ESP_LOGI(TAG, "cmd/setpoint +1 (queue)");
      uiBusQueueAdjustSetpoint(1);
      return;
    }
    if (strcasecmp(msg, "down") == 0 || strcmp(msg, "-") == 0) {
      ESP_LOGI(TAG, "cmd/setpoint -1 (queue)");
      uiBusQueueAdjustSetpoint(-1);
      return;
    }
    if ((msg[0] == '+' || msg[0] == '-') && msg[1] != '\0') {
      const int d = atoi(msg);
      if (d != 0 && d >= -10 && d <= 10) {
        ESP_LOGI(TAG, "cmd/setpoint %+d (queue)", d);
        uiBusQueueAdjustSetpoint(d);
        return;
      }
    }
    const int sp = (int)(atof(msg) + 0.5f);
    if (sp >= 15 && sp <= 65) {
      ESP_LOGI(TAG, "cmd/setpoint abs %d (queue)", sp);
      uiBusQueueSetpointC((uint8_t)sp);
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
  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_POWER)) {
    // IoT START/STOP — fronta, ne LIN z MQTT callbacku (core 0)
    bool on = false;
    if (parseOnOff(msg, &on)) {
      ESP_LOGI(TAG, "cmd/power %s (queue)", on ? "START" : "STOP");
      uiBusQueuePower(on);
    } else {
      ESP_LOGI(TAG, "cmd/power neznamy payload [%s]", msg);
    }
    return;
  }
  if (topicIs(topic, strlen(topic), MQTT_TOPIC_CMD_COMPRESSOR)) {
#if MQTT_COMPRESSOR_FORCE_ON
    ESP_LOGI(TAG, "cmd/compressor ignorovan — FORCE_ON=1 v mqtt_config.h");
    publishCompressor(true);
    return;
#else
    bool on = false;
    if (msg[0] == '\0' || strcasecmp(msg, "test") == 0) {
      on = true;
    } else if (!parseOnOff(msg, &on)) {
      ESP_LOGI(TAG, "cmd/compressor neznamy payload [%s]", msg);
      return;
    }
    s_compTest = on;
    if (on) {
      publishCompressor(true);
      ESP_LOGI(TAG, "TEST compressor ON — tele musi byt ON");
    } else {
      uint8_t b2 = 0, b3 = 0;
      publishCompressor(snapCompressorOn(&b2, &b3));
      ESP_LOGI(TAG, "TEST compressor OFF — zpet na bus");
    }
    return;
#endif
  }
}

bool subscribeAll() {
  // Fingerprint: když v logu stále vidíš cmd/quiet, na desce NENÍ tento build
  ESP_LOGI(TAG, "FW-ID=LG_Therma_7B/2026-08-03b (ocekavej SUB compressor, NE quiet)");
  const char* topics[] = {
      MQTT_TOPIC_CMD_WATCH,
      MQTT_TOPIC_CMD_POWER,
      MQTT_TOPIC_CMD_SETPOINT,
      MQTT_TOPIC_CMD_MODE,
      MQTT_TOPIC_CMD_COMPRESSOR,
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
#if MQTT_TELE_REQUIRE_WATCH
  s_watchPub = -1;
  ESP_LOGI(TAG, "tele ceka na cmd/watch ON (mobil: retain=true pri open)");
#else
#if MQTT_TELE_ENABLE
  startTeleStaggered();
#else
  ESP_LOGI(TAG, "tele DISABLED");
#endif
#endif
  // Nejdřív zpracuj retained cmd/watch z brokeru, pak srovnej tele/watch
  for (int i = 0; i < 20; ++i) {
    s_mqtt.loop();
    delay(50);
  }
#if MQTT_TELE_REQUIRE_WATCH
  publishWatchState(watchActive());
  ESP_LOGI(TAG, "po SUB watch=%d tele=%s", (int)watchActive(),
           teleAllowed() ? "on" : "idle");
#endif

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
#if !MQTT_COMPRESSOR_FORCE_ON
  s_compTest = false;
#endif
  if (s_connected) {
    // Nejdřív tele OFF (retain), pak availability
    publishTeleOfflineMarkers();
    s_mqtt.publish(MQTT_TOPIC_AVAILABILITY, "offline", true);
    s_mqtt.loop();
  }
  watchOff(false);  // už vyčištěno výše
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

bool netMqttIsWatchActive() {
#if MQTT_TELE_REQUIRE_WATCH
  return watchActive();
#else
  return false;
#endif
}

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
      if (!teleAllowed()) {
        s_teleIdx = -1;
        uiLvglSetRgbLowBandwidth(false);
      } else {
        teleTick();
        s_mqtt.loop();  // flush i mezi tele kroky
      }
    } else if (teleAllowed()) {
      telePublishChanges();
    }
#if MQTT_TELE_REQUIRE_WATCH
    if (s_watchOn && !watchActive()) {
      watchOff();
    }
#endif
    if (now - s_lastHbMs >= 5000) {
      s_lastHbMs = now;
      ESP_LOGI(TAG, "HB up=%lus watch=%d tele=%s heap=%u",
               (unsigned long)((now - s_linkUpMs) / 1000),
#if MQTT_TELE_REQUIRE_WATCH
               (int)watchActive(),
#else
               1,
#endif
               teleAllowed() ? "on" : "idle",
               (unsigned)ESP.getFreeHeap());
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
