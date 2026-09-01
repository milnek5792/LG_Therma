// MQTT přes PubSubClient + WiFiClientSecure (stejný model jako FVE).
// Na Tab5 během reconnect suspendujeme UI + loopTask (SDIO + Core1).
#include "net_mqtt_client.h"

#include "mqtt_config.h"
#include "net_wifi_mgr.h"
#include "net_ota.h"
#include "net_sdio_arbiter.h"
#include "ui_eez_model.h"
#include "ui_bus_bindings.h"
#include "app_cmd.h"
#include "climate_regulator.h"
#include "climate_scheduler.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "ui_task_ui.h"
#include "ui_display_bus.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <lwip/netdb.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

namespace {

static const char* TAG = "MQTT";

void logHeap(const char* where) {
  Serial.printf("[MQTT] heap %s: free=%u maxblk=%u dma_max=%u dma_free=%u psram=%u\n",
                where,
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                (unsigned)ESP.getFreePsram());
}

constexpr size_t kTlsDmaWant = 34 * 1024;

void* s_tlsReserve = nullptr;
size_t s_tlsReserveSz = 0;

class MqttSecureClient : public WiFiClientSecure {
 public:
  IPAddress forcedIp{};
  bool haveForcedIp = false;
  /**
   * Tab5: parent connected() dělá read(0) a/nebo nechá stale _connected=1.
   * PubSubClient pak při reconnectu přeskočí TLS (`if (_client->connected())`)
   * a čeká na CONNACK → stav -4.
   * Pravdu říká jen assumeUp (nastaví se až po úspěšném MQTT CONNECT).
   */
  volatile bool assumeUp = false;

  int connect(const char* host, uint16_t port) override {
    assumeUp = false;
    if (haveForcedIp) {
      Serial.printf("[MQTT] TLS SNI=%s ip=%s\n", host, forcedIp.toString().c_str());
      const uint32_t t0 = millis();
      const int rc = WiFiClientSecure::connect(
          forcedIp, port, host, (const char*)nullptr, (const char*)nullptr, (const char*)nullptr);
      Serial.printf("[MQTT] TLS handshake %s (%lu ms)\n",
                    rc ? "OK" : "FAIL", (unsigned long)(millis() - t0));
      return rc;
    }
    const uint32_t t0 = millis();
    const int rc = WiFiClientSecure::connect(host, port);
    Serial.printf("[MQTT] TLS handshake %s (%lu ms)\n",
                  rc ? "OK" : "FAIL", (unsigned long)(millis() - t0));
    return rc;
  }

  int connect(IPAddress ip, uint16_t port) override {
    assumeUp = false;
    const uint32_t t0 = millis();
    const int rc = WiFiClientSecure::connect(
        ip, port, MQTT_HOST, (const char*)nullptr, (const char*)nullptr, (const char*)nullptr);
    Serial.printf("[MQTT] TLS handshake %s (%lu ms)\n",
                  rc ? "OK" : "FAIL", (unsigned long)(millis() - t0));
    return rc;
  }

  uint8_t connected() override {
    // Nikdy parent — jinak stale _connected → skip TLS → CONNACK timeout
    return assumeUp ? 1 : 0;
  }

  void stop() override {
    assumeUp = false;
    WiFiClientSecure::stop();
  }
};

MqttSecureClient s_tls;
PubSubClient s_mqtt(s_tls);

SemaphoreHandle_t s_mtx = nullptr;
TaskHandle_t s_task = nullptr;
volatile bool s_requestConnect = false;
volatile bool s_requestDisconnect = false;

bool s_enabled = false;
volatile bool s_connected = false;
volatile bool s_bootSettled = false;
bool s_wantConnect = false;
char s_status[40] = "Odpojeno";
portMUX_TYPE s_statusMux = portMUX_INITIALIZER_UNLOCKED;
char s_host[64] = "";
char s_clientId[48] = "";

uint32_t s_lastTeleMs = 0;
uint32_t s_lastReconnectMs = 0;
uint32_t s_reconnectBackoffMs = 15000;
uint32_t s_linkUpMs = 0;
uint8_t s_linkFailStreak = 0;

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
  int8_t lin = -1;
  int8_t alarm = -1;
  int8_t regMode = -1;
};

TeleSnap s_pub{};
char s_pubPorucha[sizeof(uiEez.porucha_text)] = "";
int8_t s_watchPub = -1;
bool s_compTest = (MQTT_COMPRESSOR_FORCE_ON != 0);
uint32_t s_compLastPubMs = 0;

#if MQTT_TELE_REQUIRE_WATCH
bool s_watchOn = false;
uint32_t s_watchUntilMs = 0;
#endif

size_t dmaMaxBlock() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
}

void releaseTlsReserve(const char* why) {
  if (!s_tlsReserve) { return; }
  heap_caps_free(s_tlsReserve);
  s_tlsReserve = nullptr;
  Serial.printf("[MQTT] TLS reserve %u KB uvolnena (%s) dma_max=%u\n",
                (unsigned)(s_tlsReserveSz / 1024), why,
                (unsigned)dmaMaxBlock());
  s_tlsReserveSz = 0;
}

/** Během TLS: prodloužit TWDT (ne disableCore0WDT — to hází „task not found“). */
void twdtStretchForTls(bool stretch) {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = stretch ? 90000 : 5000;
  cfg.idle_core_mask = (1 << 0) | (1 << 1);
  cfg.trigger_panic = true;
  const esp_err_t err = esp_task_wdt_reconfigure(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[MQTT] TWDT reconfigure fail %d (stretch=%d)\n", (int)err, (int)stretch);
  }
}

void setStatus(const char* text) {
  char tmp[sizeof(s_status)];
  strncpy(tmp, text ? text : "", sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  if (strncmp(s_status, tmp, sizeof(s_status)) == 0) { return; }
  portENTER_CRITICAL(&s_statusMux);
  memcpy((void*)s_status, tmp, sizeof(s_status));
  portEXIT_CRITICAL(&s_statusMux);
}

void fillHostDisplay() {
  snprintf(s_host, sizeof(s_host), "%s:%d", MQTT_HOST, MQTT_PORT);
}

bool timeOkForTls() {
  const time_t now = time(nullptr);
  return now >= 1700000000;
}

bool resolveMqttIpv4(IPAddress& out) {
#if defined(MQTT_HOST_IP)
  if (out.fromString(MQTT_HOST_IP)) {
    return true;
  }
#endif
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  const int err = getaddrinfo(MQTT_HOST, nullptr, &hints, &res);
  if (err != 0 || !res || !res->ai_addr) {
    Serial.printf("[MQTT] DNS IPv4 selhalo (%d)\n", err);
    if (res) { freeaddrinfo(res); }
    return false;
  }
  const auto* addr4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
  out = IPAddress(addr4->sin_addr.s_addr);
  freeaddrinfo(res);
  return true;
}

bool mqttPublishRetain(const char* topic, const char* v) {
  if (!topic || !v || !s_mqtt.connected()) {
    return false;
  }
  bool ok = s_mqtt.publish(topic, v, true);
  s_mqtt.loop();
  if (!ok) {
    vTaskDelay(pdMS_TO_TICKS(20));
    s_mqtt.loop();
    ok = s_mqtt.publish(topic, v, true);
    s_mqtt.loop();
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

bool publishTemp(const char* topic, float v) {
  if (tempOffline(v)) {
    return false;
  }
  publishFloat(topic, v);
  return true;
}

float linTempOrNa(uint8_t raw, bool maA0) {
  if (!maA0 || raw == 0 || raw >= 100) {
    return UI_TEPLOTA_NEPLATNA;
  }
  return (float)raw;
}

void snapMqttSetpoint(float* setp) {
  if (!setp) {
    return;
  }
  if (uiEez.rezim == UI_REZIM_AUTO) {
    const float roomSp = climateRegulatorRoomSpEffective();
    *setp = (roomSp >= 16.0f && roomSp <= 24.0f) ? roomSp : UI_TEPLOTA_NEPLATNA;
    return;
  }
  lgModelLock();
  const uint8_t cil = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  lgModelUnlock();
  *setp = (cil >= 15 && cil <= 65) ? (float)cil : UI_TEPLOTA_NEPLATNA;
}

void snapLinTemps(float* inlet, float* outlet, float* outdoor, float* setp) {
  lgModelLock();
  const bool maA0 = lgMaCerstoA0();
  const uint8_t in = mVstupni;
  const uint8_t out = mVystupni;
  lgModelUnlock();
  if (inlet) {
    *inlet = linTempOrNa(in, maA0);
  }
  if (outlet) {
    *outlet = linTempOrNa(out, maA0);
  }
  if (outdoor) {
    *outdoor = uiEez.teplota_venkovni;
  }
  if (setp) {
    snapMqttSetpoint(setp);
  }
}

struct TeleModelSnap {
  int8_t lin;
  int8_t power;
  int8_t pump;
  int8_t comp;
};

bool snapCompressorOn(uint8_t* b2Out, uint8_t* b3Out);

void snapTeleFromModel(TeleModelSnap* out) {
  if (!out) {
    return;
  }
  LgModelUiSnap bus{};
  lgModelReadUiSnap(&bus);
  const bool linLive = bus.lin_live;
  const uint8_t b2 = bus.b2;
  out->lin = linLive ? 1 : 0;
  out->power = (bus.cilovy_zapnuto || bus.cekame_orig || bus.tc_pozadavek) ? 1 : 0;
  out->pump = (linLive && lgJeCerpadloZap(b2)) ? 1 : 0;
  uint8_t b2c = 0;
  uint8_t b3c = 0;
  out->comp = snapCompressorOn(&b2c, &b3c) ? 1 : 0;
}

void publishTeleOfflineMarkers() {
  if (!s_mqtt.connected()) {
    return;
  }
  mqttPublishRetain(MQTT_TOPIC_TELE_WATCH, "OFF");
  s_watchPub = 0;
  mqttPublishRetain(MQTT_TOPIC_TELE_LIN, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_POWER, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_PUMP, "OFF");
  if (s_compTest || MQTT_COMPRESSOR_FORCE_ON) {
    mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, "ON");
    s_pub.compressor = 1;
  } else {
    mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, "OFF");
    s_pub.compressor = 0;
  }
  mqttPublishRetain(MQTT_TOPIC_TELE_ALARM, "OFF");
  mqttPublishRetain(MQTT_TOPIC_TELE_PORUCHA, "");
  s_pubPorucha[0] = '\0';
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_OUTLET, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_INLET, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_OUTDOOR, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_ROOM, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_TEMP_SET, MQTT_TELE_NA);
  mqttPublishRetain(MQTT_TOPIC_TELE_REG_MODE, MQTT_TELE_NA);
  const int8_t keepComp = s_pub.compressor;
  s_pub = TeleSnap{};
  s_pub.lin = 0;
  s_pub.power = 0;
  s_pub.pump = 0;
  s_pub.compressor = keepComp;
  s_pub.alarm = 0;
}

void startTeleStaggered(void);

#if MQTT_TELE_REQUIRE_WATCH
bool watchActive() {
  if (!s_watchOn) {
    return false;
  }
  return (int32_t)(s_watchUntilMs - millis()) > 0;
}

void publishWatchState(bool on) {
  if (!s_mqtt.connected()) {
    return;
  }
  const int8_t v = on ? 1 : 0;
  if (s_watchPub == v) {
    return;
  }
  s_watchPub = v;
  mqttPublishRetain(MQTT_TOPIC_TELE_WATCH, on ? "ON" : "OFF");
}

void bumpWatch(uint32_t holdMs = MQTT_WATCH_IDLE_MS) {
  const bool was = watchActive();
  s_watchOn = true;
  const uint32_t until = millis() + holdMs;
  if ((int32_t)(until - s_watchUntilMs) > 0) {
    s_watchUntilMs = until;
  }
  netSdioBumpWatch(holdMs);
  publishWatchState(true);
  if (!was) {
    startTeleStaggered();
  }
}

void watchOff(bool clearTopics = true) {
  const bool was = s_watchOn || s_teleIdx >= 0 || s_watchPub == 1;
  s_watchOn = false;
  s_watchUntilMs = 0;
  s_teleIdx = -1;
  netSdioWatchOff();
  if (clearTopics && was) {
    publishTeleOfflineMarkers();
  } else {
    publishWatchState(false);
  }
}

bool teleAllowed() {
#if MQTT_TELE_ENABLE
  return watchActive();
#else
  return false;
#endif
}
#else
bool watchActive() { return MQTT_TELE_ENABLE != 0; }
void bumpWatch(uint32_t holdMs) { netSdioBumpWatch(holdMs); }
void watchOff(bool) { netSdioWatchOff(); }
bool teleAllowed() { return MQTT_TELE_ENABLE != 0; }
void publishWatchState(bool) {}
#endif

bool nearlyEq(float a, float b) {
  return fabsf(a - b) < 0.05f;
}

int8_t mqttRegModeCode(void) {
  return (uiEez.rezim == UI_REZIM_AUTO) ? 1 : 0;
}

const char* mqttRegModePayload(void) {
  return (uiEez.rezim == UI_REZIM_AUTO) ? "room" : "water";
}

void publishRegMode(void) {
  const char* v = mqttRegModePayload();
  publishStr(MQTT_TOPIC_TELE_REG_MODE, v);
  s_pub.regMode = mqttRegModeCode();
}

void publishSetpoint(void) {
  float setp = UI_TEPLOTA_NEPLATNA;
  snapMqttSetpoint(&setp);
  if (!tempOffline(setp)) {
    publishFloat(MQTT_TOPIC_TELE_TEMP_SET, setp);
    s_pub.setp = setp;
  }
}

bool snapCompressorOn(uint8_t* b2Out, uint8_t* b3Out) {
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
  return uiEez.sig_kompresor || (maA0 && lgJeKompresorBezi(b3));
}

void publishCompressor(bool on) {
#if MQTT_COMPRESSOR_FORCE_ON
  on = true;
  s_compTest = true;
#else
  if (s_compTest) {
    on = true;
  }
#endif
  mqttPublishRetain(MQTT_TOPIC_TELE_COMPRESSOR, on ? "ON" : "OFF");
  s_pub.compressor = on ? 1 : 0;
  s_compLastPubMs = millis();
}

void publishOutlet(float v) {
  if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTLET, v)) {
    s_pub.outlet = v;
  }
}

void publishTeleOne(int idx) {
  float inlet = UI_TEPLOTA_NEPLATNA;
  float outlet = UI_TEPLOTA_NEPLATNA;
  float outdoor = UI_TEPLOTA_NEPLATNA;
  float setp = UI_TEPLOTA_NEPLATNA;

  switch (idx) {
    case 0: {
      TeleModelSnap t{};
      snapTeleFromModel(&t);
      publishStr(MQTT_TOPIC_TELE_LIN, t.lin ? "ON" : "OFF");
      s_pub.lin = t.lin;
      break;
    }
    case 1: {
      TeleModelSnap t{};
      snapTeleFromModel(&t);
      publishStr(MQTT_TOPIC_TELE_POWER, t.power ? "ON" : "OFF");
      s_pub.power = t.power;
      break;
    }
    case 2: {
      TeleModelSnap t{};
      snapTeleFromModel(&t);
      publishStr(MQTT_TOPIC_TELE_PUMP, t.pump ? "ON" : "OFF");
      s_pub.pump = t.pump;
      break;
    }
    case 3: {
      uint8_t b2 = 0, b3 = 0;
      publishCompressor(snapCompressorOn(&b2, &b3));
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
      }
      break;
    case 6:
      snapLinTemps(&inlet, &outlet, &outdoor, &setp);
      if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTDOOR, outdoor)) {
        s_pub.outdoor = outdoor;
      }
      break;
    case 7:
      publishSetpoint();
      break;
    case 8:
      if (publishTemp(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni)) {
        s_pub.room = uiEez.teplota_vnitrni;
      }
      break;
    case 9:
      publishStr(MQTT_TOPIC_TELE_ALARM, uiEez.sig_alarm ? "ON" : "OFF");
      s_pub.alarm = uiEez.sig_alarm ? 1 : 0;
      strncpy(s_pubPorucha, uiEez.porucha_text, sizeof(s_pubPorucha) - 1);
      s_pubPorucha[sizeof(s_pubPorucha) - 1] = '\0';
      publishStr(MQTT_TOPIC_TELE_PORUCHA, s_pubPorucha);
      break;
    case 10:
      publishRegMode();
      break;
    default:
      break;
  }
}

static const int kTeleSyncCount = 11;

void teleTick() {
  if (!s_mqtt.connected() || s_teleIdx < 0) {
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
    s_teleIdx = -1;
  }
}

void startTeleStaggered() {
  if (!s_mqtt.connected() || s_teleIdx >= 0 || !teleAllowed()) {
    return;
  }
  s_pub = TeleSnap{};
  s_teleIdx = 0;
  s_teleStartMs = millis();
  teleTick();
}

void telePublishChanges() {
  if (!s_mqtt.connected() || s_teleIdx >= 0 || !teleAllowed()) {
    return;
  }
  const uint32_t now = millis();
  if (now - s_lastChangeCheckMs < MQTT_TELE_CHANGE_MS) {
    return;
  }
  s_lastChangeCheckMs = now;

  TeleModelSnap t{};
  snapTeleFromModel(&t);
  const bool compRefresh = (now - s_compLastPubMs) >= 3000;
  if (t.lin != s_pub.lin) {
    publishStr(MQTT_TOPIC_TELE_LIN, t.lin ? "ON" : "OFF");
    s_pub.lin = t.lin;
  }
  if (t.power != s_pub.power) {
    publishStr(MQTT_TOPIC_TELE_POWER, t.power ? "ON" : "OFF");
    s_pub.power = t.power;
  }
  if (t.pump != s_pub.pump) {
    publishStr(MQTT_TOPIC_TELE_PUMP, t.pump ? "ON" : "OFF");
    s_pub.pump = t.pump;
  }
  if (t.comp != s_pub.compressor || compRefresh) {
    publishCompressor(t.comp != 0);
  }
  const int8_t al = uiEez.sig_alarm ? 1 : 0;
  if (al != s_pub.alarm) {
    publishStr(MQTT_TOPIC_TELE_ALARM, al ? "ON" : "OFF");
    s_pub.alarm = al;
  }
  if (strcmp(uiEez.porucha_text, s_pubPorucha) != 0) {
    strncpy(s_pubPorucha, uiEez.porucha_text, sizeof(s_pubPorucha) - 1);
    s_pubPorucha[sizeof(s_pubPorucha) - 1] = '\0';
    publishStr(MQTT_TOPIC_TELE_PORUCHA, s_pubPorucha);
  }

  float inlet = UI_TEPLOTA_NEPLATNA;
  float outlet = UI_TEPLOTA_NEPLATNA;
  float outdoor = UI_TEPLOTA_NEPLATNA;
  float setp = UI_TEPLOTA_NEPLATNA;
  snapLinTemps(&inlet, &outlet, &outdoor, &setp);

  const int8_t rm = mqttRegModeCode();
  if (rm != s_pub.regMode) {
    publishRegMode();
    publishSetpoint();
    return;
  }
  if (!tempOffline(outlet) && !nearlyEq(outlet, s_pub.outlet)) {
    publishOutlet(outlet);
    return;
  }
  if (!tempOffline(inlet) && !nearlyEq(inlet, s_pub.inlet)) {
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_INLET, inlet)) {
      s_pub.inlet = inlet;
    }
    return;
  }
  if (!tempOffline(outdoor) && !nearlyEq(outdoor, s_pub.outdoor)) {
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_OUTDOOR, outdoor)) {
      s_pub.outdoor = outdoor;
    }
    return;
  }
  if (!tempOffline(setp) && !nearlyEq(setp, s_pub.setp)) {
    publishSetpoint();
    return;
  }
  if (!tempOffline(uiEez.teplota_vnitrni) &&
      !nearlyEq(uiEez.teplota_vnitrni, s_pub.room)) {
    if (publishTemp(MQTT_TOPIC_TELE_TEMP_ROOM, uiEez.teplota_vnitrni)) {
      s_pub.room = uiEez.teplota_vnitrni;
    }
  }
}

bool topicIs(const char* topic, int topicLen, const char* expect) {
  const int n = (int)strlen(expect);
  return topicLen == n && strncmp(topic, expect, n) == 0;
}

bool parseOnOff(const char* data, int len, bool* onOut) {
  if (!data || len <= 0 || !onOut) { return false; }
  char buf[16];
  const int n = len < (int)sizeof(buf) - 1 ? len : (int)sizeof(buf) - 1;
  memcpy(buf, data, n);
  buf[n] = '\0';
  for (int i = 0; i < n; ++i) {
    if (buf[i] >= 'a' && buf[i] <= 'z') { buf[i] = (char)(buf[i] - 'a' + 'A'); }
  }
  if (strcmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "TRUE") == 0) {
    *onOut = true;
    return true;
  }
  if (strcmp(buf, "OFF") == 0 || strcmp(buf, "0") == 0 || strcmp(buf, "FALSE") == 0) {
    *onOut = false;
    return true;
  }
  return false;
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

void handleIncoming(const char* topic, int topicLen, const char* data, int dataLen) {
  if (!topic || topicLen <= 0) { return; }

  char msg[64];
  const int n = (dataLen < (int)sizeof(msg) - 1) ? dataLen : (int)sizeof(msg) - 1;
  if (data && n > 0) {
    memcpy(msg, data, n);
  }
  msg[n] = '\0';
  trimInPlace(msg);
  Serial.printf("[MQTT] RX %.*s = [%s]\n", topicLen, topic, msg);

  auto parseOnOffMsg = [](const char* m, bool* onOut) -> bool {
    if (!m || !onOut) {
      return false;
    }
    if (strcasecmp(m, "ON") == 0 || strcmp(m, "1") == 0 || strcasecmp(m, "true") == 0 ||
        strcasecmp(m, "start") == 0) {
      *onOut = true;
      return true;
    }
    if (strcasecmp(m, "OFF") == 0 || strcmp(m, "0") == 0 || strcasecmp(m, "false") == 0 ||
        strcasecmp(m, "stop") == 0) {
      *onOut = false;
      return true;
    }
    return false;
  };

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_WATCH)) {
    bool on = false;
    if (msg[0] == '\0' || strcasecmp(msg, "watch") == 0) {
      on = true;
    } else if (!parseOnOffMsg(msg, &on)) {
      return;
    }
    if (on) {
      bumpWatch();
    } else {
      watchOff();
    }
    return;
  }

  bumpWatch();

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_SETPOINT)) {
    const bool autoMode = (uiEez.rezim == UI_REZIM_AUTO);
    if (strcasecmp(msg, "up") == 0 || strcmp(msg, "+") == 0) {
      uiBusQueueAdjustSetpoint(autoMode ? 5 : 1);
      return;
    }
    if (strcasecmp(msg, "down") == 0 || strcmp(msg, "-") == 0) {
      uiBusQueueAdjustSetpoint(autoMode ? -5 : -1);
      return;
    }
    if ((msg[0] == '+' || msg[0] == '-') && msg[1] != '\0') {
      char* end = nullptr;
      const float df = strtof(msg, &end);
      if (end != msg && *end == '\0' && df != 0.0f) {
        if (autoMode) {
          const int tenths = (int)lroundf(df * 10.0f);
          if (tenths != 0 && tenths >= -100 && tenths <= 100) {
            uiBusQueueAdjustSetpoint(tenths);
          }
        } else {
          const int d = (int)lroundf(df);
          if (d != 0 && d >= -10 && d <= 10) {
            uiBusQueueAdjustSetpoint(d);
          }
        }
        return;
      }
    }
    const float spf = (float)atof(msg);
    if (autoMode) {
      if (spf >= 18.0f && spf <= 24.0f) {
        appCmdEnqueueSetpointAbs((int)lroundf(spf * 10.0f), UI_SP_SRC_MQTT);
      }
    } else {
      const int sp = (int)(spf + 0.5f);
      if (sp >= REG_T_WATER_MIN_C && sp <= REG_T_WATER_MAX_C) {
        uiBusQueueSetpointC((uint8_t)sp);
      }
    }
    return;
  }

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_MODE)) {
    if (strcasecmp(msg, "room") == 0 || strcasecmp(msg, "auto") == 0) {
      uiBusQueueSetRegulationAuto(true);
    } else if (strcasecmp(msg, "water") == 0 || strcasecmp(msg, "manual") == 0 ||
               strcasecmp(msg, "vystupni") == 0 || strcasecmp(msg, "voda") == 0) {
      uiBusQueueSetRegulationAuto(false);
    }
    return;
  }

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_POWER)) {
    bool on = false;
    if (parseOnOffMsg(msg, &on)) {
      uiBusQueuePower(on);
    }
    return;
  }

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_COMPRESSOR)) {
#if MQTT_COMPRESSOR_FORCE_ON
    publishCompressor(true);
    return;
#else
    bool on = false;
    if (msg[0] == '\0' || strcasecmp(msg, "test") == 0) {
      on = true;
    } else if (!parseOnOffMsg(msg, &on)) {
      return;
    }
    s_compTest = on;
    uint8_t b2 = 0, b3 = 0;
    publishCompressor(on ? true : snapCompressorOn(&b2, &b3));
    return;
#endif
  }

  if (topicIs(topic, topicLen, MQTT_TOPIC_CMD_QUIET)) {
    bool on = false;
    if (parseOnOffMsg(msg, &on)) {
      if (on && !uiEez.sig_utlum) {
        climateTichyManualToggle();
      } else if (!on && uiEez.sig_utlum) {
        climateTichyManualToggle();
      }
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  handleIncoming(topic, topic ? (int)strlen(topic) : 0,
                 reinterpret_cast<const char*>(payload), (int)length);
}

void applyPendingCommands() {}

bool s_displayHeldForTls = false;
TaskHandle_t s_asyncSuspended = nullptr;

void resumeCore1AfterMqtt() {
  if (netOtaIsBusy()) {
    return;
  }
  if (s_displayHeldForTls) {
    uiDisplayBusUnlock();
    s_displayHeldForTls = false;
  }
  if (s_asyncSuspended) {
    eTaskState st = eTaskGetState(s_asyncSuspended);
    if (st == eSuspended) {
      vTaskResume(s_asyncSuspended);
    }
    s_asyncSuspended = nullptr;
  }
  netSdioSetTlsBusy(false);
  netSdioClearUiFreeze();
  lgTaskUiResume();
  TaskHandle_t loopTask = xTaskGetHandle("loopTask");
  if (loopTask) {
    eTaskState st = eTaskGetState(loopTask);
    if (st == eSuspended) {
      vTaskResume(loopTask);
    }
  }
}

/** Po úspěšném MQTT: pusť Core1, ale drž LVGL freeze (unfreeze shazuje TLS). */
void resumeCore1SoftAfterMqtt(uint32_t freezeMs) {
  if (netOtaIsBusy()) {
    return;
  }
  if (s_displayHeldForTls) {
    uiDisplayBusUnlock();
    s_displayHeldForTls = false;
  }
  if (s_asyncSuspended) {
    eTaskState st = eTaskGetState(s_asyncSuspended);
    if (st == eSuspended) {
      vTaskResume(s_asyncSuspended);
    }
    s_asyncSuspended = nullptr;
  }
  netSdioSetTlsBusy(false);
  // NE ClearUiFreeze — unfreeze storm zabíjí spojení (~2 s po resume)
  netSdioHoldUiFreeze(freezeMs);
  lgTaskUiResume();
  TaskHandle_t loopTask = xTaskGetHandle("loopTask");
  if (loopTask) {
    eTaskState st = eTaskGetState(loopTask);
    if (st == eSuspended) {
      vTaskResume(loopTask);
    }
  }
}

void suspendCore1ForMqtt() {
  // 1) Flag dřív než suspend — loop() musí stihnout odejít z M5.update()
  //    (jinak Core 1 Store fault @ 0x500d2000 během TLS).
  s_connected = false;
  s_asyncSuspended = nullptr;
  netSdioSetTlsBusy(true);
  Serial.println("[MQTT] tlsBusy — cekam az loop opusti M5.update");
  Serial.flush();
  vTaskDelay(pdMS_TO_TICKS(400));

  // loopTask NECHÁME běžet — custom touch v loop(); jinak UI „zamrzne“ na celý reconnect.
  lgTaskUiSuspend();
  TaskHandle_t asyncTask = xTaskGetHandle("async_tcp");
  if (asyncTask) {
    vTaskSuspend(asyncTask);
    s_asyncSuspended = asyncTask;
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  if (uiDisplayBusLock(pdMS_TO_TICKS(500))) {
    s_displayHeldForTls = true;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("[MQTT] core1 parked (UI+display, loop bezi)");
  Serial.flush();
}

void disconnectMqttLocked() {
  const bool hadSession = s_connected || s_tls.assumeUp;
#if !MQTT_COMPRESSOR_FORCE_ON
  s_compTest = false;
#endif
  s_tls.assumeUp = false;
  s_connected = false;
  if (hadSession && s_mqtt.connected()) {
    publishTeleOfflineMarkers();
    s_mqtt.publish(MQTT_TOPIC_AVAILABILITY, "offline", true);
    s_mqtt.loop();
  }
  watchOff(false);
  s_teleIdx = -1;
  s_mqtt.disconnect();
  s_tls.stop();
  netSdioWatchOff();
  if (hadSession) {
    netSdioSetMqttSession(false);
  }
  netSdioEndMqtt();
  resumeCore1AfterMqtt();
}

/** Úplné zavření TLS/MQTT bez UI resume (session padla / před reconnectem). */
void hardStopMqttSocket(const char* why) {
  s_tls.assumeUp = false;
  s_connected = false;
  s_mqtt.disconnect();
  s_tls.stop();
  Serial.printf("[MQTT] hard stop (%s) state=%d\n", why, s_mqtt.state());
}

/** Pád živé session — zruš i watch (oko / rychlé tele). */
void hardStopMqttSession(const char* why) {
  hardStopMqttSocket(why);
  watchOff(false);
  s_teleIdx = -1;
#if !MQTT_COMPRESSOR_FORCE_ON
  s_compTest = false;
#endif
}

void fillClientId() {
  const uint32_t macLo = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFull);
  snprintf(s_clientId, sizeof(s_clientId), "%s_%06X", MQTT_CLIENT_ID_PREFIX,
           (unsigned)macLo);
}

bool subscribeAll() {
  static const char* const kTopics[] = {
      MQTT_TOPIC_CMD_WATCH,
      MQTT_TOPIC_CMD_POWER,
      MQTT_TOPIC_CMD_SETPOINT,
      MQTT_TOPIC_CMD_MODE,
      MQTT_TOPIC_CMD_COMPRESSOR,
      MQTT_TOPIC_CMD_QUIET,
  };
  for (size_t i = 0; i < sizeof(kTopics) / sizeof(kTopics[0]); ++i) {
    if (!s_mqtt.subscribe(kTopics[i])) {
      Serial.printf("[MQTT] SUB fail %s\n", kTopics[i]);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    s_mqtt.loop();
    Serial.printf("[MQTT] SUB %s\n", kTopics[i]);
  }
  return true;
}

// FVE-style reconnect: ALPN mqtt + setInsecure + PubSubClient.connect
bool reconnectMqttLocked() {
  fillHostDisplay();
  fillClientId();
  Serial.printf("[MQTT] clientId=%s\n", s_clientId);

  if (!netSdioCanMqtt()) {
    setStatus("Čekám na UI...");
    return false;
  }

  if (!timeOkForTls()) {
    setStatus("Čekám na čas/NTP");
    Serial.println("[MQTT] TLS potrebuje platny cas (NTP/RTC)");
    return false;
  }
  if (!netWifiIsConnected()) {
    setStatus("Čekám na Wi-Fi...");
    return false;
  }

  suspendCore1ForMqtt();

  if (!s_tlsReserve) {
    netMqttReserveTlsMemory();
  }

  IPAddress ip;
  if (!resolveMqttIpv4(ip)) {
    setStatus("DNS selhalo");
    resumeCore1AfterMqtt();
    return false;
  }
  Serial.printf("[MQTT] DNS IPv4 %s -> %s\n", MQTT_HOST, ip.toString().c_str());
  s_tls.forcedIp = ip;
  s_tls.haveForcedIp = true;

  if (!netSdioTryBeginMqtt()) {
    setStatus("Čekám na SDIO...");
    resumeCore1AfterMqtt();
    return false;
  }
  netSdioSetMqttSession(true);
  WiFi.setSleep(false);

  // Vynutit čistý socket — jinak PubSubClient přeskočí TLS a skončí -4
  hardStopMqttSocket("pred connect");
  vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_SETTLE_MS));
  releaseTlsReserve("pred handshake");

  // FVE3: setInsecure + lokální ALPN "mqtt" (bez CA certifikátu)
  s_tls.setInsecure();
  {
    const char* alpnProtocols[] = {"mqtt", nullptr};
    s_tls.setAlpnProtocols(alpnProtocols);
  }
  Serial.println("[MQTT] TLS sifrovane, bez overeni certifikatu");
  // Handshake max 30 s; read/available krátký timeout — jinak available() blokuje celé sekundy
  s_tls.setHandshakeTimeout(30);
  s_tls.setTimeout(1000);

  // Hostname (ne IP) — EMQX Cloud potřebuje SNI. DNS IPv4 už ověřen výše.
  s_mqtt.setServer(MQTT_HOST, (uint16_t)MQTT_PORT);
  s_mqtt.setCallback(mqttCallback);
  s_mqtt.setBufferSize(1024);
  s_mqtt.setKeepAlive(60);
  s_mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

  setStatus("Připojování...");
  Serial.printf("[MQTT] Pripojuji k EMQX (timeout=%ds)...", MQTT_SOCKET_TIMEOUT_S);
  logHeap("pred connect");
  vTaskDelay(pdMS_TO_TICKS(200));

  // TLS handshake blokuje Core 0 — dočasně delší TWDT (ne disable IDLE0).
  twdtStretchForTls(true);
  const uint32_t tConn0 = millis();
  // Bez Will — menší CONNECT (FVE taky bez will při debug); availability pošleme po OK
  const bool ok = s_mqtt.connect(s_clientId, MQTT_USER, MQTT_PASSWORD);
  twdtStretchForTls(false);
  Serial.printf("[MQTT] connect() trvalo %lu ms, ok=%d state=%d\n",
                (unsigned long)(millis() - tConn0), (int)ok, s_mqtt.state());

  if (!ok) {
    const int st = s_mqtt.state();
    Serial.printf(" CHYBA, stav: %d%s\n", st,
                  st == -4 ? " (CONNACK timeout)" : "");
    setStatus(st == -4 ? "Timeout" : "MQTT fail");
    hardStopMqttSocket("connect fail");
    netSdioSetMqttSession(false);
    netSdioEndMqtt();
    resumeCore1AfterMqtt();
    return false;
  }

  Serial.println(" OK");
  // Teprve teď smí connected() vracet 1 (subscribe/publish/loop)
  s_tls.assumeUp = true;
  s_connected = true;

  if (!subscribeAll()) {
    Serial.println("[MQTT] subscribe selhalo");
    hardStopMqttSocket("sub fail");
    netSdioSetMqttSession(false);
    netSdioEndMqtt();
    resumeCore1AfterMqtt();
    setStatus("SUB fail");
    return false;
  }

  s_mqtt.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
  s_mqtt.loop();

#if MQTT_TELE_REQUIRE_WATCH
  s_watchPub = -1;
#else
  startTeleStaggered();
#endif
  for (int i = 0; i < 20; ++i) {
    s_mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
#if MQTT_TELE_REQUIRE_WATCH
  publishWatchState(watchActive());
#endif

  s_bootSettled = true;
  s_linkFailStreak = 0;
  s_reconnectBackoffMs = 15000;
  s_linkUpMs = millis();
  s_lastTeleMs = millis();
  setStatus("Připojeno");

  // Drž LVGL freeze — plný unfreeze shazuje TLS. Touch může freeze zrušit dotykem.
  resumeCore1SoftAfterMqtt(8000);
  Serial.println("[MQTT] UI soft-resume (LVGL freeze 8s, touch=clear)");
  Serial.flush();
  return true;
}

void mqttWorker(void* /*arg*/) {
  Serial.println("[MQTT] worker start (PubSubClient, FVE-style)");
  bool bootReady = false;
  bool bootConnectSent = false;
  uint32_t bootReadyMs = 0;

  for (;;) {
    if (!bootReady && s_enabled && netWifiIsConnected() && timeOkForTls()) {
      bootReady = true;
      bootReadyMs = millis();
      s_wantConnect = true;
      Serial.println("[MQTT] boot: WiFi+NTP OK — cekam 3s pred connect");
      setStatus("Připojování...");
    }
    if (bootReady && !bootConnectSent && (millis() - bootReadyMs >= 3000)) {
      bootConnectSent = true;
      s_requestConnect = true;
      Serial.println("[MQTT] boot: pripojuji");
    }

    if (s_requestDisconnect) {
      s_requestDisconnect = false;
      if (xSemaphoreTake(s_mtx, portMAX_DELAY) == pdTRUE) {
        disconnectMqttLocked();
        setStatus("Vypnuto");
        xSemaphoreGive(s_mtx);
      }
    }

    if (s_requestConnect) {
      if (!s_enabled || !netWifiIsConnected()) {
        s_requestConnect = false;
        setStatus(!netWifiIsConnected() ? "Nejdřív Wi-Fi" : "Vypnuto");
      } else if (!bootReady) {
        setStatus("Čeká na Wi-Fi");
      } else if (!netSdioCanMqtt()) {
        setStatus("Čekám na UI...");
      } else if (xSemaphoreTake(s_mtx, portMAX_DELAY) == pdTRUE) {
        s_requestConnect = false;
        netSdioBumpWatchDefault();
        setStatus("Připojování...");
        reconnectMqttLocked();
        s_lastReconnectMs = millis();
        xSemaphoreGive(s_mtx);
      }
    }

    // Keepalive + tele + reconnect (bez falešných ztrát)
    if (s_enabled && s_wantConnect && netWifiIsConnected() && bootReady) {
      if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
        const uint32_t now = millis();

        if (s_connected) {
          // Vždy loop — jinak keepalive umře a dostaneme -4 při reconnectu
          s_mqtt.loop();
          const bool linked = s_mqtt.connected() && (s_mqtt.state() == MQTT_CONNECTED);
          if (!linked) {
            // Po UI resume bývá krátký šum — ale stop()/assumeUp=0 = opravdu mrtvé
            if (!s_tls.assumeUp) {
              Serial.printf("[MQTT] session padla state=%d\n", s_mqtt.state());
              hardStopMqttSession("lost assumeUp=0");
              s_linkFailStreak = 0;
              netSdioSetMqttSession(false);
              netSdioEndMqtt();
              setStatus("Odpojeno");
              s_lastReconnectMs = now;
            } else if ((millis() - s_linkUpMs) >= 15000) {
              s_linkFailStreak++;
              if (s_linkFailStreak >= 15) {
                Serial.printf("[MQTT] session padla state=%d\n", s_mqtt.state());
                hardStopMqttSession("lost streak");
                s_linkFailStreak = 0;
                netSdioSetMqttSession(false);
                netSdioEndMqtt();
                setStatus("Odpojeno");
                s_lastReconnectMs = now;
              }
            } else {
              Serial.printf("[MQTT] transient state=%d (grace)\n", s_mqtt.state());
            }
          } else {
            s_linkFailStreak = 0;
            if (s_teleIdx >= 0) {
              teleTick();
              s_mqtt.loop();
            } else if (teleAllowed()) {
              telePublishChanges();
            }
#if MQTT_TELE_REQUIRE_WATCH
            if (s_watchOn && !watchActive()) {
              watchOff();
            }
#endif
            if (s_connected) {
              setStatus("Připojeno");
            }
          }
        } else if ((now - s_lastReconnectMs) >= s_reconnectBackoffMs) {
          if (!netSdioCanMqtt()) {
            setStatus("Čekám na UI...");
            s_lastReconnectMs = now - s_reconnectBackoffMs + 2000;
          } else {
          s_lastReconnectMs = now;
          setStatus("Připojování...");
          Serial.printf("[MQTT] reconnect (backoff %lu ms)...\n",
                        (unsigned long)s_reconnectBackoffMs);
          if (reconnectMqttLocked()) {
            s_reconnectBackoffMs = 15000;
          } else if (s_reconnectBackoffMs < 120000) {
            s_reconnectBackoffMs = (s_reconnectBackoffMs < 30000)
                                      ? 30000
                                      : (s_reconnectBackoffMs + 30000);
          }
          }
        }
        xSemaphoreGive(s_mtx);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(s_connected ? 100 : 200));
    netSdioTick();
  }
}

}  // namespace

void netMqttReserveTlsMemory() {
  if (s_tlsReserve) { return; }
  static const size_t kTries[] = {
      40 * 1024, 36 * 1024, 32 * 1024, 28 * 1024};
  for (size_t i = 0; i < sizeof(kTries) / sizeof(kTries[0]); ++i) {
    const size_t n = kTries[i];
    void* p = heap_caps_malloc(n, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!p) {
      p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (p) {
      s_tlsReserve = p;
      s_tlsReserveSz = n;
      Serial.printf("[MQTT] TLS reserve %u KB: OK (dma_max=%u)\n",
                    (unsigned)(n / 1024), (unsigned)dmaMaxBlock());
      return;
    }
  }
  Serial.println("[MQTT] TLS reserve FAIL — TLS muze OOM");
}

void netMqttInit() {
  s_enabled = true;
  s_connected = false;
  s_wantConnect = true;
  s_bootSettled = false;
  fillHostDisplay();
  setStatus("Čeká na Wi-Fi");
  s_mqtt.setCallback(mqttCallback);
  s_mqtt.setBufferSize(1024);
  if (!s_mtx) {
    s_mtx = xSemaphoreCreateMutex();
  }
  if (!s_task) {
    constexpr uint32_t kStackWords = 8192;  // 32 KB INTERNAL
    static StaticTask_t s_tcb;
    static StackType_t* s_stack = nullptr;
    if (!s_stack) {
      s_stack = (StackType_t*)heap_caps_malloc(
          kStackWords * sizeof(StackType_t),
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_stack) {
      s_task = xTaskCreateStaticPinnedToCore(
          mqttWorker, "mqtt_w", kStackWords, nullptr, 1,
          s_stack, &s_tcb, 0);
      Serial.println("[MQTT] worker: static INTERNAL stack (PubSubClient)");
    } else {
      xTaskCreatePinnedToCore(
          mqttWorker, "mqtt_w", 32768, nullptr, 1, &s_task, 0);
      Serial.println("[MQTT] worker: fallback xTaskCreate");
    }
  }
  logHeap("init");
}

void netMqttSetEnabled(bool on) {
  s_enabled = on;
  if (!on) {
    s_wantConnect = false;
    s_requestDisconnect = true;
    setStatus("Vypnuto");
    return;
  }
  s_wantConnect = true;
  setStatus("Připojování...");
  s_requestConnect = true;
}

bool netMqttIsEnabled() { return s_enabled; }

void netMqttConnect() {
  if (!netWifiIsEnabled() || !netWifiIsConnected()) {
    setStatus("Nejdřív Wi-Fi");
    Serial.println("[MQTT] nejdriv Wi-Fi");
    return;
  }
  s_enabled = true;
  s_wantConnect = true;
  s_lastReconnectMs = 0;
  fillHostDisplay();
  netSdioBumpWatchDefault();
  setStatus("Připojování...");
  s_requestConnect = true;
}

bool netMqttIsConnected() { return s_connected; }

bool netMqttBootSettled() { return s_bootSettled; }

const char* netMqttStatus() { return s_status; }

const char* netMqttHost() { return s_host; }

void netMqttTick() {
  applyPendingCommands();
}

bool netMqttIsBusy() {
  return s_requestConnect || s_wantConnect;
}

bool netMqttIsWatchActive() {
#if MQTT_TELE_REQUIRE_WATCH
  return watchActive();
#else
  return false;
#endif
}
