// Seeed XIAO ESP32-C3 — SwitchBot Meter → UART bridge pro Tab5
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "bridge_ota.h"
#include "h2_uart_protocol.h"

// XIAO ESP32-C3 D6/D7 — UART k Tab5 (M5-Bus G6/G7)
#ifndef BRIDGE_TX_PIN
#define BRIDGE_TX_PIN 21  // D6
#endif
#ifndef BRIDGE_RX_PIN
#define BRIDGE_RX_PIN 20  // D7
#endif
#ifndef UART_BAUD
#define UART_BAUD 115200
#endif
#ifndef BLE_METER_MAC_DEFAULT
#define BLE_METER_MAC_DEFAULT "EC:6F:03:86:1E:6B"
#endif
#ifndef BLE_OUTDOOR_MAC_DEFAULT
#define BLE_OUTDOOR_MAC_DEFAULT "E8:76:C3:46:66:14"
#endif
// Status LED: Xiao nemá uživatelskou LED → -1; DevKit/SuperMini WS2812 GPIO8
#ifndef STATUS_LED_PIN
#if defined(ARDUINO_XIAO_ESP32C3)
#define STATUS_LED_PIN -1
#elif defined(RGB_BUILTIN)
#define STATUS_LED_PIN RGB_BUILTIN
#else
#define STATUS_LED_PIN 8
#endif
#endif
#if STATUS_LED_PIN >= 0
#define BRIDGE_HAS_STATUS_LED 1
#else
#define BRIDGE_HAS_STATUS_LED 0
#endif

namespace {

constexpr uint16_t kSbServiceUuid = 0xFD3D;
constexpr uint16_t kSbCompanyId = 0x0969;
constexpr uint32_t kScanMs = 10000;
constexpr uint32_t kPollScanMs = 10000;  // oba senzory v jednom scanu (Meter vysílá ~1–4 s)
constexpr uint32_t kPollIntervalMs = 60000;
constexpr uint32_t kMissRetryMs = 20000;   // venku/pokoj minul scan → dřívější opakování
constexpr int kFoundMax = 8;
constexpr uint8_t kLedDim = 12;  // nízký jas WS2812

struct MeterReading {
  bool valid = false;
  float temp = NAN;
  float hum = NAN;
  int batt = -1;
  int rssi = 0;
};

struct FoundEntry {
  uint8_t mac[6]{};
  float temp = NAN;
  int rssi = 0;
  bool valid = false;
};

Preferences s_prefs;
uint8_t s_roomMac[6]{};
uint8_t s_outMac[6]{};
bool s_roomOk = false;
bool s_outOk = false;

float s_roomTemp = NAN;
float s_roomHum = NAN;
int s_roomBatt = -1;
int s_roomRssi = 0;

float s_outTemp = NAN;
float s_outHum = NAN;
int s_outBatt = -1;
int s_outRssi = 0;

bool s_scanBusy = false;
bool s_discoveryMode = false;
bool s_scanWantOk = false;
bool s_scanWantTelemetry = false;
uint32_t s_scanStartedMs = 0;
FoundEntry s_found[kFoundMax];
int s_foundCount = 0;

uint32_t s_lastPollMs = 0;
uint32_t s_lastSendMs = 0;
uint32_t s_lastBleOkMs = 0;
uint32_t s_missRetryAtMs = 0;
bool s_roomHitScan = false;
bool s_outHitScan = false;
uint32_t s_ledPulseUntilMs = 0;
uint32_t s_ledHbMs = 0;
bool s_ledHbOn = false;

char s_rxLine[96];
size_t s_rxLen = 0;

void ledWrite(uint8_t r, uint8_t g, uint8_t b) {
#if BRIDGE_HAS_STATUS_LED
  // WS2812 GRB (SuperMini GPIO8)
  rgbLedWrite(STATUS_LED_PIN, g, r, b);
#else
  (void)r;
  (void)g;
  (void)b;
#endif
}

void ledOff() {
  ledWrite(0, 0, 0);
}

void ledPulse(uint8_t r, uint8_t g, uint8_t b, uint32_t ms) {
  s_ledPulseUntilMs = millis() + ms;
  ledWrite(r, g, b);
}

void ledTick() {
#if !BRIDGE_HAS_STATUS_LED
  return;
#endif
  const uint32_t now = millis();

  // Během blokujícího scanu LED řídí runScan() — tady nic
  if (s_scanBusy) {
    return;
  }

  if (s_ledPulseUntilMs != 0) {
    if ((int32_t)(now - s_ledPulseUntilMs) < 0) {
      return;
    }
    s_ledPulseUntilMs = 0;
  }

  const bool haveData =
      (!isnan(s_roomTemp) || !isnan(s_outTemp)) &&
      (s_lastBleOkMs != 0) && (now - s_lastBleOkMs) < 180000UL;

  // Heartbeat každých 2 s (~80 ms)
  if (now - s_ledHbMs >= 2000) {
    s_ledHbMs = now;
    s_ledHbOn = true;
  }
  if (s_ledHbOn) {
    if (now - s_ledHbMs < 80) {
      if (haveData) {
        ledWrite(0, kLedDim, 0);  // zelená
      } else {
        ledWrite(0, 0, kLedDim);  // modrá = čeká na data
      }
      return;
    }
    s_ledHbOn = false;
  }

  if (haveData) {
    ledWrite(0, 3, 0);
  } else {
    ledOff();
  }
}

bool parseMac(const char* s, uint8_t out[6]) {
  unsigned v[6];
  if (!s || sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &v[0], &v[1], &v[2],
                   &v[3], &v[4], &v[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; ++i) {
    out[i] = (uint8_t)v[i];
  }
  bool allZero = true;
  for (int i = 0; i < 6; ++i) {
    if (out[i] != 0) {
      allZero = false;
      break;
    }
  }
  return !allZero;
}

void macToStr(const uint8_t mac[6], char* buf, size_t len) {
  snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

bool macMatch(const uint8_t* nativeLe, const uint8_t target[6]) {
  if (!nativeLe) {
    return false;
  }
  bool fwd = true;
  bool rev = true;
  for (int i = 0; i < 6; ++i) {
    if (nativeLe[i] != target[i]) {
      fwd = false;
    }
    if (nativeLe[i] != target[5 - i]) {
      rev = false;
    }
  }
  return fwd || rev;
}

/** Venkovní Meter (WoSensorTHO): service data 0xFD3D má jen 3 B — baterie v p[2]. */
bool parseFd3dOutdoorShort(const uint8_t* p, size_t n, int rssi, MeterReading* out) {
  if (!p || n < 3 || !out) {
    return false;
  }
  const int batt = (int)(p[2] & 0x7F);
  if (batt > 100) {
    return false;
  }
  out->valid = true;
  out->batt = batt;
  out->rssi = rssi;
  return true;
}

bool parseFd3dService(const uint8_t* p, size_t n, int rssi, MeterReading* out) {
  if (!p || n < 6 || !out) {
    return false;
  }
  const int batt = (int)(p[2] & 0x7F);
  const bool aboveZero = (p[4] & 0x80) != 0;
  float t = (float)(p[4] & 0x7F) + (float)(p[3] & 0x0F) * 0.1f;
  if (!aboveZero) {
    t = -t;
  }
  const float hum = (float)(p[5] & 0x7F);
  if (t <= -40.0f || t >= 85.0f || hum > 100.0f || batt > 100) {
    return false;
  }
  out->valid = true;
  out->batt = batt;
  out->temp = t;
  out->hum = hum;
  out->rssi = rssi;
  return true;
}

bool parseMfr0969(const uint8_t* p, size_t n, int rssi, MeterReading* out) {
  if (!p || n < 11 || !out) {
    return false;
  }
  auto tryTemp = [&](size_t i0, size_t i1, size_t i2) -> bool {
    if (n <= i2) {
      return false;
    }
    const bool aboveZero = (p[i1] & 0x80) != 0;
    float t = (float)(p[i1] & 0x7F) + (float)(p[i0] & 0x0F) * 0.1f;
    if (!aboveZero) {
      t = -t;
    }
    const float hum = (float)(p[i2] & 0x7F);
    if (t <= -40.0f || t >= 85.0f || hum > 100.0f) {
      return false;
    }
    out->valid = true;
    out->temp = t;
    out->hum = hum;
    out->rssi = rssi;
    out->batt = -1;
    return true;
  };
  if (tryTemp(10, 11, 12)) {
    return true;
  }
  return tryTemp(8, 9, 10);
}

bool parseAdv(const uint8_t* adv, size_t len, int rssi, MeterReading* out) {
  if (!adv || len < 4 || !out) {
    return false;
  }
  MeterReading fd3d{};
  MeterReading mfr{};
  bool gotFd3d = false;
  bool gotMfr = false;
  size_t i = 0;
  while (i + 1 < len) {
    const uint8_t fieldLen = adv[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) {
      break;
    }
    const uint8_t fieldType = adv[i + 1];
    const uint8_t* data = &adv[i + 2];
    const size_t dataLen = fieldLen - 1;

    if (fieldType == 0x16 && dataLen >= 5) {
      const uint16_t uuid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (uuid == kSbServiceUuid) {
        const uint8_t* payload = data + 2;
        const size_t plen = dataLen - 2;
        MeterReading tmp{};
        if (plen >= 6 && parseFd3dService(payload, plen, rssi, &tmp)) {
          fd3d = tmp;
          gotFd3d = true;
        } else if (plen >= 3 && parseFd3dOutdoorShort(payload, plen, rssi, &tmp)) {
          fd3d = tmp;
          gotFd3d = true;
        }
      }
    }
    if (fieldType == 0xFF && dataLen >= 11) {
      const uint16_t cid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      MeterReading tmp{};
      if (cid == kSbCompanyId && parseMfr0969(data, dataLen, rssi, &tmp)) {
        mfr = tmp;
        gotMfr = true;
      }
    }
    i += fieldLen + 1;
  }
  if (!gotFd3d && !gotMfr) {
    return false;
  }
  if (gotFd3d) {
    *out = fd3d;
  } else {
    *out = mfr;
  }
  // Sloučit: venkovní meter dává T/H v 0x0969 a baterii v krátkém 0xFD3D (často jiný paket).
  if (gotFd3d && gotMfr) {
    if (isnan(out->temp)) {
      out->temp = mfr.temp;
    }
    if (isnan(out->hum) || out->hum < 0.0f) {
      out->hum = mfr.hum;
    }
    if (out->batt < 0 && fd3d.batt >= 0) {
      out->batt = fd3d.batt;
    }
    if (out->batt < 0 && mfr.batt >= 0) {
      out->batt = mfr.batt;
    }
  }
  return true;
}

bool advHasSwitchBot(const uint8_t* adv, size_t len) {
  if (!adv || len < 4) {
    return false;
  }
  size_t i = 0;
  while (i + 1 < len) {
    const uint8_t fieldLen = adv[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) {
      break;
    }
    const uint8_t fieldType = adv[i + 1];
    const uint8_t* data = &adv[i + 2];
    const size_t dataLen = fieldLen - 1;
    if (fieldType == 0x16 && dataLen >= 2) {
      const uint16_t uuid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (uuid == kSbServiceUuid) {
        return true;
      }
    }
    if (fieldType == 0xFF && dataLen >= 2) {
      const uint16_t cid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (cid == kSbCompanyId) {
        return true;
      }
    }
    i += fieldLen + 1;
  }
  return false;
}

void uartPrint(const char* line) {
  Serial1.print(line);
  Serial.print(line);
}

void uartPrintf(const char* fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  uartPrint(buf);
}

void mergeRoomReading(const MeterReading& reading, int rssi) {
  if (!isnan(reading.temp)) {
    s_roomTemp = reading.temp;
  }
  s_roomRssi = rssi;
  if (!isnan(reading.hum) && reading.hum >= 0.0f && reading.hum <= 100.0f) {
    s_roomHum = reading.hum;
  }
  if (reading.batt >= 0) {
    s_roomBatt = reading.batt;
  }
}

void mergeOutReading(const MeterReading& reading, int rssi) {
  if (!isnan(reading.temp)) {
    s_outTemp = reading.temp;
  }
  s_outRssi = rssi;
  if (!isnan(reading.hum) && reading.hum >= 0.0f && reading.hum <= 100.0f) {
    s_outHum = reading.hum;
  }
  if (reading.batt >= 0) {
    s_outBatt = reading.batt;
  }
}

void formatTelemetryLine(char* buf, size_t len, bool outdoor) {
  const float t = outdoor ? s_outTemp : s_roomTemp;
  const float h = outdoor ? s_outHum : s_roomHum;
  const int batt = outdoor ? s_outBatt : s_roomBatt;
  const int rssi = outdoor ? s_outRssi : s_roomRssi;
  size_t n = 0;
  if (outdoor) {
    n = snprintf(buf, len, "OUT T=%.1f", t);
  } else {
    n = snprintf(buf, len, "T=%.1f", t);
  }
  if (!isnan(h) && h >= 0.0f && h <= 100.0f) {
    n += snprintf(buf + n, len - n, " H=%.0f", h);
  }
  if (batt >= 0) {
    n += snprintf(buf + n, len - n, " B=%d", batt);
  }
  snprintf(buf + n, len - n, " R=%d", rssi);
}

void sendTelemetryRoom() {
  if (isnan(s_roomTemp)) {
    return;
  }
  char line[64];
  formatTelemetryLine(line, sizeof(line), false);
  uartPrintf("%s\n", line);
}

void sendTelemetryOutdoor() {
  if (isnan(s_outTemp)) {
    return;
  }
  char line[64];
  formatTelemetryLine(line, sizeof(line), true);
  uartPrintf("%s\n", line);
}

void sendTelemetry(bool roomHit, bool outHit) {
  if (!roomHit && !outHit) {
    return;
  }
  if ((millis() - s_lastSendMs) < 500) {
    return;
  }
  s_lastSendMs = millis();
  if (roomHit) {
    sendTelemetryRoom();
  }
  if (outHit) {
    sendTelemetryOutdoor();
  }
  if (roomHit || outHit) {
    ledPulse(0, kLedDim, kLedDim, 80);  // cyan = telemetrie na Tab5
  }
}

bool scanTargetsComplete() {
  const bool roomDone = !s_roomOk || s_roomHitScan;
  const bool outDone = !s_outOk || s_outHitScan;
  return roomDone && outDone;
}

void maybeStopScanEarly() {
  if (s_discoveryMode || !scanTargetsComplete()) {
    return;
  }
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan && scan->isScanning()) {
    scan->stop();
  }
}

void addFound(const uint8_t mac[6], float temp, int rssi) {
  for (int i = 0; i < s_foundCount; ++i) {
    if (macMatch(mac, s_found[i].mac)) {
      s_found[i].temp = temp;
      s_found[i].rssi = rssi;
      s_found[i].valid = true;
      return;
    }
  }
  if (s_foundCount >= kFoundMax) {
    return;
  }
  memcpy(s_found[s_foundCount].mac, mac, 6);
  s_found[s_foundCount].temp = temp;
  s_found[s_foundCount].rssi = rssi;
  s_found[s_foundCount].valid = true;
  ++s_foundCount;
}

void reportFoundList() {
  for (int i = 0; i < s_foundCount; ++i) {
    char macStr[20];
    macToStr(s_found[i].mac, macStr, sizeof(macStr));
    uartPrintf("FOUND %d %s T=%.1f R=%d\n", i + 1, macStr, s_found[i].temp,
               s_found[i].rssi);
  }
  uartPrintf("SCAN DONE n=%d\n", s_foundCount);
}

void sendCfg() {
  char room[20];
  char out[20];
  macToStr(s_roomMac, room, sizeof(room));
  macToStr(s_outMac, out, sizeof(out));
  uartPrintf("CFG ROOM=%s OUT=%s\n", room, out);
}

void saveMacs() {
  char room[20];
  char out[20];
  macToStr(s_roomMac, room, sizeof(room));
  macToStr(s_outMac, out, sizeof(out));
  s_prefs.putString("room_mac", room);
  s_prefs.putString("out_mac", out);
}

void loadMacs() {
  String room = s_prefs.getString("room_mac", BLE_METER_MAC_DEFAULT);
  String out = s_prefs.getString("out_mac", BLE_OUTDOOR_MAC_DEFAULT);
  s_roomOk = parseMac(room.c_str(), s_roomMac);
  s_outOk = parseMac(out.c_str(), s_outMac);
  if (!s_roomOk) {
    memset(s_roomMac, 0, sizeof(s_roomMac));
  }
  if (!s_outOk) {
    memset(s_outMac, 0, sizeof(s_outMac));
    s_outOk = false;
  }
}

bool setRoomMac(const uint8_t mac[6]) {
  memcpy(s_roomMac, mac, 6);
  s_roomOk = true;
  saveMacs();
  return true;
}

bool setOutMac(const uint8_t mac[6]) {
  memcpy(s_outMac, mac, 6);
  s_outOk = true;
  saveMacs();
  return true;
}

bool setRoomByIndex(int idx1) {
  if (idx1 < 1 || idx1 > s_foundCount) {
    return false;
  }
  return setRoomMac(s_found[idx1 - 1].mac);
}

bool setOutByIndex(int idx1) {
  if (idx1 < 1 || idx1 > s_foundCount) {
    return false;
  }
  return setOutMac(s_found[idx1 - 1].mac);
}

uint32_t pollScanMs() {
  // Venkovní senzor bývá dál / vysílá řídce — delší scan když je nakonfigurovaný.
  return s_outOk ? 15000u : kPollScanMs;
}

class ScanCb : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (!d) {
      return;
    }
    const uint8_t* native = d->getAddress().getVal();
    const int rssi = d->getRSSI();
    const std::vector<uint8_t>& payloadVec = d->getPayload();
    const uint8_t* payload = payloadVec.data();
    const size_t plen = payloadVec.size();
    const bool matchRoom = !s_discoveryMode && s_roomOk && macMatch(native, s_roomMac);
    const bool matchOut = !s_discoveryMode && s_outOk && macMatch(native, s_outMac);
    MeterReading reading{};

    if (!parseAdv(payload, plen, rssi, &reading)) {
      if (!matchRoom && !matchOut) {
        return;
      }
      Serial.printf("[BLE] MAC hit rssi=%d ale parse fail\n", rssi);
      return;
    }

    if (s_discoveryMode) {
      if (advHasSwitchBot(payload, plen)) {
        addFound(native, reading.temp, rssi);
      }
      return;
    }

    if (matchRoom) {
      mergeRoomReading(reading, rssi);
      s_roomHitScan = true;
      s_lastBleOkMs = millis();
      maybeStopScanEarly();
    }
    if (matchOut) {
      mergeOutReading(reading, rssi);
      s_outHitScan = true;
      s_lastBleOkMs = millis();
      Serial.printf("[BLE] outdoor hit T=%.1f rssi=%d\n", reading.temp, rssi);
      maybeStopScanEarly();
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override;
};

ScanCb s_scanCb;

void finishScan() {
  if (!s_scanBusy) {
    return;
  }

  const bool discovery = s_discoveryMode;
  const bool wantTelem = s_scanWantTelemetry;
  const bool wantOk = s_scanWantOk;
  const bool roomHit = s_roomHitScan;
  const bool outHit = s_outHitScan;

  s_discoveryMode = false;
  s_scanBusy = false;
  s_scanWantTelemetry = false;
  s_scanWantOk = false;
  s_scanStartedMs = 0;
  s_roomHitScan = false;
  s_outHitScan = false;

  if (discovery) {
    reportFoundList();
  }
  if (wantTelem) {
    sendTelemetry(roomHit, outHit);
    if (s_outOk && !outHit) {
      char outStr[20];
      macToStr(s_outMac, outStr, sizeof(outStr));
      Serial.printf("[BLE] poll MISS outdoor MAC=%s — retry\n", outStr);
      s_missRetryAtMs = millis() + kMissRetryMs;
    }
    if (s_roomOk && !roomHit) {
      Serial.println("[BLE] poll MISS room — retry");
      if (s_missRetryAtMs == 0) {
        s_missRetryAtMs = millis() + kMissRetryMs;
      }
    }
  }
  if (wantOk) {
    uartPrint("OK\n");
  }
  ledOff();
  Serial.printf("[BLE] scan end discovery=%d found=%d\n", (int)discovery,
                s_foundCount);
}

void ScanCb::onScanEnd(const NimBLEScanResults& results, int reason) {
  (void)results;
  Serial.printf("[BLE] onScanEnd reason=%d\n", reason);
  finishScan();
}

bool beginScan(uint32_t ms, bool discovery, bool wantOk, bool wantTelem) {
  if (s_scanBusy) {
    Serial.println("[BLE] scan busy");
    if (wantOk) {
      uartPrint("ERR BUSY\n");
    }
    return false;
  }

  s_scanBusy = true;
  s_discoveryMode = discovery;
  s_scanWantOk = wantOk;
  s_scanWantTelemetry = wantTelem;
  s_scanStartedMs = millis();
  s_roomHitScan = false;
  s_outHitScan = false;
  s_ledPulseUntilMs = 0;
  ledWrite(kLedDim + 6, kLedDim / 3, 0);  // oranžová = skenuju

  if (discovery) {
    s_foundCount = 0;
    for (int i = 0; i < kFoundMax; ++i) {
      s_found[i] = FoundEntry{};
    }
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&s_scanCb, false);
  scan->setActiveScan(true);
  scan->setInterval(160);
  scan->setWindow(120);
  scan->setDuplicateFilter(false);

  // start() je asynchronní — výsledek v onScanEnd / finishScan
  if (!scan->start(ms, false, true)) {
    Serial.println("[BLE] scan start failed");
    s_scanBusy = false;
    s_discoveryMode = false;
    s_scanWantOk = false;
    s_scanWantTelemetry = false;
    ledOff();
    if (wantOk) {
      uartPrint("ERR SCAN\n");
    }
    return false;
  }
  Serial.printf("[BLE] scan start %lu ms discovery=%d\n", (unsigned long)ms,
                (int)discovery);
  return true;
}

void abortScanForWifi(void) {
  if (!s_scanBusy) {
    return;
  }
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan && scan->isScanning()) {
    scan->stop();
  }
  finishScan();
}

bool handleWifiCommand(char* line) {
  if (!line) {
    return false;
  }
  if (strcmp(line, H2_CMD_WIFI_OFF) == 0 || strcmp(line, H2_CMD_OTA_STOP) == 0) {
    abortScanForWifi();
    bridgeOtaStopWifi();
    uartPrint("OK\n");
    return true;
  }
  if (strcmp(line, H2_CMD_WIFI_START) == 0) {
    abortScanForWifi();
    if (bridgeOtaStartWifiStored()) {
      uartPrint("OK\n");
    } else {
      uartPrint("ERR WIFI\n");
    }
    return true;
  }
  if (strncmp(line, "WIFI\t", 5) == 0) {
    char* ssid = line + 5;
    char* pass = strchr(ssid, '\t');
    if (!pass || !ssid[0]) {
      uartPrint("ERR WIFI\n");
      return true;
    }
    *pass++ = '\0';
    abortScanForWifi();
    if (bridgeOtaStartWifi(ssid, pass)) {
      uartPrint("OK\n");
    } else {
      uartPrint("ERR WIFI\n");
    }
    return true;
  }
  return false;
}

void handleCommand(char* line) {
  if (!line || !line[0]) {
    return;
  }

  if (handleWifiCommand(line)) {
    return;
  }

  if (strcmp(line, "SCAN") == 0) {
    Serial.println("[BLE] SCAN");
    beginScan(kScanMs, true, true, false);
    return;
  }
  if (strcmp(line, "POLL") == 0) {
    Serial.println("[BLE] POLL");
    s_missRetryAtMs = 0;
    if (s_roomOk || s_outOk) {
      beginScan(pollScanMs(), false, true, true);
    } else {
      uartPrint("OK\n");
    }
    return;
  }
  if (strcmp(line, "GET CFG") == 0) {
    sendCfg();
    return;
  }
  if (strncmp(line, "SET ROOM=", 9) == 0) {
    const char* val = line + 9;
    if (strchr(val, ':') != nullptr) {
      uint8_t mac[6];
      if (parseMac(val, mac)) {
        setRoomMac(mac);
        uartPrint("OK\n");
        sendCfg();
        return;
      }
    } else {
      const int idx = atoi(val);
      if (setRoomByIndex(idx)) {
        uartPrint("OK\n");
        sendCfg();
        return;
      }
    }
    uartPrint("ERR ROOM\n");
    return;
  }
  if (strncmp(line, "SET OUT=", 8) == 0) {
    const char* val = line + 8;
    if (strcmp(val, "0") == 0 || strcasecmp(val, "OFF") == 0) {
      memset(s_outMac, 0, sizeof(s_outMac));
      s_outOk = false;
      s_outTemp = NAN;
      saveMacs();
      uartPrint("OK\n");
      sendCfg();
      return;
    }
    if (strchr(val, ':') != nullptr) {
      uint8_t mac[6];
      if (parseMac(val, mac)) {
        setOutMac(mac);
        uartPrint("OK\n");
        sendCfg();
        return;
      }
    } else {
      const int idx = atoi(val);
      if (setOutByIndex(idx)) {
        uartPrint("OK\n");
        sendCfg();
        return;
      }
    }
    uartPrint("ERR OUT\n");
    return;
  }
  uartPrintf("ERR unknown: %s\n", line);
}

void feedRxByte(char c) {
  if (c == '\r') {
    return;
  }
  if (c == '\n') {
    if (s_rxLen > 0) {
      s_rxLine[s_rxLen] = '\0';
      ledPulse(0, kLedDim + 4, 0, 50);  // zelený flash = příkaz z Tab5
      handleCommand(s_rxLine);
      s_rxLen = 0;
    }
    return;
  }
  if (s_rxLen + 1 < sizeof(s_rxLine)) {
    s_rxLine[s_rxLen++] = c;
  } else {
    s_rxLen = 0;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  ledOff();
  ledPulse(0, 0, kLedDim, 200);  // start = modrá

  Serial1.begin(UART_BAUD, SERIAL_8N1, BRIDGE_RX_PIN, BRIDGE_TX_PIN);

  bridgeOtaInit(uartPrint);

  s_prefs.begin("h2_bridge", false);
  loadMacs();

  char room[20];
  char out[20];
  macToStr(s_roomMac, room, sizeof(room));
  macToStr(s_outMac, out, sizeof(out));
  Serial.printf("[BLE] Xiao C3 UART TX=%d RX=%d room=%s out=%s LED=%d\n",
                BRIDGE_TX_PIN, BRIDGE_RX_PIN, room, out, STATUS_LED_PIN);

  NimBLEDevice::init("");
  sendCfg();

  // První teploty hned po startu (ne až po intervalu pollu)
  if (s_roomOk || s_outOk) {
    Serial.println("[BLE] boot POLL");
    beginScan(pollScanMs(), false, false, true);
    s_lastPollMs = millis();
  }
}

void loop() {
  while (Serial1.available() > 0) {
    feedRxByte((char)Serial1.read());
  }

  bridgeOtaTick();

  const uint32_t now = millis();

  if (bridgeOtaWifiBusy()) {
    ledTick();
    delay(20);
    return;
  }

  // Watchdog: onScanEnd někdy nedorazí
  if (s_scanBusy && s_scanStartedMs != 0 &&
      (now - s_scanStartedMs) > (pollScanMs() + 3000UL)) {
    Serial.println("[BLE] scan watchdog — force end");
    NimBLEDevice::getScan()->stop();
    finishScan();
  }

  if (!s_scanBusy && s_missRetryAtMs != 0 && now >= s_missRetryAtMs &&
      (s_roomOk || s_outOk)) {
    s_missRetryAtMs = 0;
    Serial.println("[BLE] miss retry scan");
    beginScan(pollScanMs(), false, false, true);
  }

  if (!s_scanBusy && (s_roomOk || s_outOk) &&
      (now - s_lastPollMs) >= kPollIntervalMs) {
    s_lastPollMs = now;
    s_missRetryAtMs = 0;
    beginScan(pollScanMs(), false, false, true);
  }

  ledTick();
  delay(20);
}
