// climate_ble_room.cpp — SwitchBot x2: NimBLE (pouze 7B)
#include "lg_board.h"
#if LG_HAS_NATIVE_BLE

#include "climate_ble_room.h"

#include "ble_config.h"
#include "net_mqtt_client.h"
#include "net_wifi_mgr.h"
#include "ui_eez_model.h"
#include "ui_ui_lvgl.h"

#include <Arduino.h>
#include <esp_coexist.h>
#include <esp_log.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if LG_THERMA_BLE_ROOM
#include <NimBLEDevice.h>
#endif

namespace {

static const char* TAG = "BLE";

#if LG_THERMA_BLE_ROOM

constexpr uint16_t kSbServiceUuid = 0xFD3D;
constexpr uint16_t kSbCompanyId = 0x0969;
constexpr uint32_t kOfflineMs = 5 * 60 * 1000UL;

enum class Phase : uint8_t {
  Idle = 0,
  BleInit,
  Scanning,
  Cleanup,
};

struct MeterReading {
  bool valid = false;
  float temp = NAN;
  float hum = NAN;
  int batt = -1;
  int rssi = 0;
};

struct MeterState {
  bool configured = false;
  uint8_t mac[6] = {};
  bool ok = false;
  float temp = NAN;
  float hum = NAN;
  int batt = -1;
  int rssi = 0;
  uint32_t lastOkMs = 0;
  bool hitThisScan = false;
};

MeterState s_room;
MeterState s_outdoor;

uint32_t s_lastPollMs = 0;
uint32_t s_phaseMs = 0;
Phase s_phase = Phase::Idle;
bool s_nimbleUp = false;
bool s_forcePoll = false;
bool s_firstPollPending = true;

bool parseMac(const char* s, uint8_t out[6]) {
  unsigned v[6];
  if (!s || sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &v[0], &v[1], &v[2],
                   &v[3], &v[4], &v[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; ++i) {
    out[i] = (uint8_t)v[i];
  }
  // 00:00:00:00:00:00 = nekonfigurováno
  bool allZero = true;
  for (int i = 0; i < 6; ++i) {
    if (out[i] != 0) {
      allZero = false;
      break;
    }
  }
  return !allZero;
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
    if (t <= -40.0f || t >= 85.0f || hum < 1.0f || hum > 99.0f) {
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
  if (tryTemp(8, 9, 10)) {
    return true;
  }
  return false;
}

void logHex(const char* tag, const uint8_t* p, size_t n) {
  char hex[64];
  size_t pos = 0;
  for (size_t i = 0; i < n && pos + 3 < sizeof(hex); ++i) {
    pos += (size_t)snprintf(hex + pos, sizeof(hex) - pos, "%02X", p[i]);
  }
  ESP_LOGI(TAG, "%s len=%u %s", tag, (unsigned)n, hex);
}

bool parseAdv(const uint8_t* adv, size_t len, int rssi, MeterReading* out) {
  if (!adv || len < 4 || !out) {
    return false;
  }
  MeterReading tmp{};
  bool got = false;
  size_t i = 0;
  while (i + 1 < len) {
    const uint8_t fieldLen = adv[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) {
      break;
    }
    const uint8_t fieldType = adv[i + 1];
    const uint8_t* data = &adv[i + 2];
    const size_t dataLen = fieldLen - 1;

    if (fieldType == 0x16 && dataLen >= 8) {
      const uint16_t uuid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (uuid == kSbServiceUuid) {
        logHex("svc FD3D", data + 2, dataLen - 2);
        if (parseFd3dService(data + 2, dataLen - 2, rssi, &tmp)) {
          got = true;
        }
      }
    }
    if (fieldType == 0xFF && dataLen >= 11) {
      const uint16_t cid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (cid == kSbCompanyId) {
        logHex("mfr 0969", data, dataLen);
        if (parseMfr0969(data, dataLen, rssi, &tmp)) {
          got = true;
        }
      }
    }
    i += fieldLen + 1;
  }
  if (got) {
    *out = tmp;
  }
  return got;
}

bool advHasSwitchBotUuid(const uint8_t* adv, size_t len) {
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

void formatMac(const uint8_t* le, char* out, size_t outLen) {
  if (!le || outLen < 18) {
    if (out && outLen) {
      out[0] = '\0';
    }
    return;
  }
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X", le[5], le[4], le[3],
           le[2], le[1], le[0]);
}

void applyReading(MeterState* st, const MeterReading& r) {
  if (!st || !r.valid) {
    return;
  }
  st->ok = true;
  st->temp = r.temp;
  st->hum = r.hum;
  st->batt = r.batt;
  st->rssi = r.rssi;
  st->lastOkMs = millis();
  st->hitThisScan = true;
}

bool scanComplete() {
  const bool roomDone = !s_room.configured || s_room.hitThisScan;
  const bool outDone = !s_outdoor.configured || s_outdoor.hitThisScan;
  return roomDone && outDone;
}

void maybeStopScanEarly() {
  if (!scanComplete()) {
    return;
  }
  NimBLEScan* sc = NimBLEDevice::getScan();
  if (sc && sc->isScanning()) {
    sc->stop();
  }
}

class ScanCbs : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (!d) {
      return;
    }
    const ble_addr_t* ba = d->getAddress().getBase();
    if (!ba) {
      return;
    }
    const std::vector<uint8_t>& payload = d->getPayload();
    if (!advHasSwitchBotUuid(payload.data(), payload.size())) {
      return;
    }

    MeterReading reading{};
    const bool parsed =
        parseAdv(payload.data(), payload.size(), d->getRSSI(), &reading);

    char macStr[20];
    formatMac(ba->val, macStr, sizeof(macStr));
    const bool matchRoom =
        s_room.configured && macMatch(ba->val, s_room.mac);
    const bool matchOut =
        s_outdoor.configured && macMatch(ba->val, s_outdoor.mac);

    ESP_LOGI(TAG, "SB cand %s rssi=%d parsed=%d room=%d out=%d T=%.1f", macStr,
             d->getRSSI(), (int)parsed, (int)matchRoom, (int)matchOut,
             parsed ? (double)reading.temp : -999.0);

#if BLE_ACCEPT_ANY_SWITCHBOT
    if (parsed) {
      if (!s_room.hitThisScan) {
        applyReading(&s_room, reading);
      } else if (s_outdoor.configured && !s_outdoor.hitThisScan) {
        applyReading(&s_outdoor, reading);
      }
      maybeStopScanEarly();
    }
#else
    if (parsed && matchRoom) {
      applyReading(&s_room, reading);
      ESP_LOGI(TAG, "hit ROOM T=%.1f", (double)reading.temp);
      maybeStopScanEarly();
    } else if (parsed && matchOut) {
      applyReading(&s_outdoor, reading);
      ESP_LOGI(TAG, "hit OUTDOOR T=%.1f", (double)reading.temp);
      maybeStopScanEarly();
    }
#endif
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    ESP_LOGI(TAG, "scan end reason=%d count=%d room=%d out=%d", reason,
             results.getCount(), (int)s_room.hitThisScan,
             (int)s_outdoor.hitThisScan);
  }
};

ScanCbs s_scanCbs;

void expireIfStale(MeterState* st) {
  if (!st || !st->ok) {
    return;
  }
  if (!st->lastOkMs || (millis() - st->lastOkMs) > kOfflineMs) {
    st->ok = false;
    st->temp = NAN;
  }
}

void applyToUi() {
  expireIfStale(&s_room);
  expireIfStale(&s_outdoor);

  if (s_room.ok && !isnan(s_room.temp)) {
    uiEez.teplota_vnitrni = s_room.temp;
  } else {
    uiEez.teplota_vnitrni = UI_TEPLOTA_NEPLATNA;
  }

  if (s_outdoor.ok && !isnan(s_outdoor.temp)) {
    uiEez.teplota_venkovni = s_outdoor.temp;
  } else {
    uiEez.teplota_venkovni = UI_TEPLOTA_NEPLATNA;
  }

  // sig_ble = pokojový senzor OK (hlavní)
  uiEez.sig_ble = s_room.ok;
}

void enterPhase(Phase p) {
  s_phase = p;
  s_phaseMs = millis();
}

void finishPoll() {
  applyToUi();
  if (!s_room.hitThisScan) {
    ESP_LOGW(TAG, "poll MISS room");
  }
  if (s_outdoor.configured && !s_outdoor.hitThisScan) {
    ESP_LOGW(TAG, "poll MISS outdoor");
  }
  if (s_firstPollPending) {
    if (s_room.hitThisScan || !s_room.configured) {
      s_firstPollPending = false;
    } else {
      // První scan selhal — MQTT dál čeká, rychlý retry místo 120 s
      const uint32_t now = millis();
      s_lastPollMs = now - (BLE_POLL_INTERVAL_MS - BLE_FAIL_RETRY_MS);
      ESP_LOGW(TAG, "first poll neuspel — retry za %lus",
               (unsigned long)(BLE_FAIL_RETRY_MS / 1000));
    }
  }
  enterPhase(Phase::Cleanup);
}

void bleStopScanOnly() {
  if (!s_nimbleUp) {
    return;
  }
  NimBLEScan* sc = NimBLEDevice::getScan();
  if (sc && sc->isScanning()) {
    sc->stop();
  }
  if (sc) {
    sc->clearResults();
  }
  ESP_LOGI(TAG, "scan stopped (NimBLE stays up)");
}

void bleDeinitSafe() {
  if (!s_nimbleUp) {
    return;
  }
  bleStopScanOnly();
  NimBLEDevice::deinit(true);
  s_nimbleUp = false;
  ESP_LOGI(TAG, "NimBLE deinit");
}

#endif  // LG_THERMA_BLE_ROOM

}  // namespace

void climateBleInit(void) {
#if LG_THERMA_BLE_ROOM
  s_room = MeterState{};
  s_outdoor = MeterState{};
  s_room.configured = parseMac(BLE_METER_MAC, s_room.mac);
  s_outdoor.configured = parseMac(BLE_OUTDOOR_MAC, s_outdoor.mac);
  s_lastPollMs = 0;
  s_phase = Phase::Idle;
  s_forcePoll = false;
  s_firstPollPending = true;
  uiEez.teplota_vnitrni = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_venkovni = UI_TEPLOTA_NEPLATNA;
  uiEez.sig_ble = false;
  ESP_LOGI(TAG, "init room=%d MAC=%s outdoor=%d MAC=%s interval=%lus",
           (int)s_room.configured, BLE_METER_MAC, (int)s_outdoor.configured,
           BLE_OUTDOOR_MAC, (unsigned long)(BLE_POLL_INTERVAL_MS / 1000));
#else
  ESP_LOGI(TAG, "BLE room disabled");
#endif
}

void climateBleRequestNow(void) {
#if LG_THERMA_BLE_ROOM
  s_forcePoll = true;
  ESP_LOGI(TAG, "poll requested (UI)");
#endif
}

void climateBleStatusText(char* buf, size_t buflen) {
  if (!buf || buflen == 0) {
    return;
  }
#if LG_THERMA_BLE_ROOM
  if (s_phase != Phase::Idle) {
    snprintf(buf, buflen, "Skenuji SwitchBot...");
    return;
  }
  char roomPart[40];
  char outPart[40];
  if (s_room.ok && !isnan(s_room.temp)) {
    snprintf(roomPart, sizeof(roomPart), "in %.1fC", (double)s_room.temp);
  } else {
    snprintf(roomPart, sizeof(roomPart), "in ---");
  }
  if (!s_outdoor.configured) {
    snprintf(outPart, sizeof(outPart), "out n/a");
  } else if (s_outdoor.ok && !isnan(s_outdoor.temp)) {
    snprintf(outPart, sizeof(outPart), "out %.1fC", (double)s_outdoor.temp);
  } else {
    snprintf(outPart, sizeof(outPart), "out ---");
  }
  snprintf(buf, buflen, "%s | %s", roomPart, outPart);
#else
  snprintf(buf, buflen, "BLE vypnuto");
#endif
}

void climateBleTick(void) {
#if LG_THERMA_BLE_ROOM
  if (!s_room.configured && !s_outdoor.configured) {
    return;
  }

  const uint32_t now = millis();

  switch (s_phase) {
    case Phase::Idle: {
      if (netWifiIsBusy()) {
        return;
      }
      // Pokojový senzor je lokální — scan neblokovat čekáním na Wi‑Fi/MQTT.
      // Během TLS handshake nech scan počkat (kromě prvního boot poll).
      if (netMqttIsBusy() && !s_firstPollPending && !s_forcePoll) {
        return;
      }
      const bool due = s_forcePoll || s_firstPollPending ||
                       (s_lastPollMs != 0 &&
                        (now - s_lastPollMs >= BLE_POLL_INTERVAL_MS));
      if (!due) {
        return;
      }
      const bool first = s_firstPollPending;
      s_forcePoll = false;
      s_lastPollMs = now;
      s_room.hitThisScan = false;
      s_outdoor.hitThisScan = false;
      ESP_LOGI(TAG, "poll BEGIN%s (%s, interval=%lus)",
               first ? " first" : "",
               netWifiIsConnected() ? "WiFi OK" : "bez WiFi",
               (unsigned long)(BLE_POLL_INTERVAL_MS / 1000));
      uiLvglSetRgbLowBandwidth(true);
      enterPhase(Phase::BleInit);
      break;
    }

    case Phase::BleInit: {
      // Paralelní scan: během skenu dej BLE víc RF (Wi‑Fi zůstává připojené)
      esp_coex_preference_set(ESP_COEX_PREFER_BT);
      // NimBLE flood „Duplicate“ přes USB CDC hladoví loop → mrtvý touch
      esp_log_level_set("NimBLE", ESP_LOG_WARN);
      esp_log_level_set("NimBLEDevice", ESP_LOG_WARN);
      esp_log_level_set("NimBLEScan", ESP_LOG_WARN);
      ESP_LOGI(TAG, "NimBLE %s...",
               NimBLEDevice::isInitialized() ? "reuse" : "init");
      if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("");
      }
      s_nimbleUp = true;
      NimBLEScan* scan = NimBLEDevice::getScan();
      scan->setScanCallbacks(&s_scanCbs, false);
      scan->setActiveScan(true);
      scan->setInterval(160);
      scan->setWindow(120);
      scan->setDuplicateFilter(false);  // oba senzory
      ESP_LOGI(TAG, "scan start %d ms room=%s out=%s", BLE_SCAN_MS,
               BLE_METER_MAC, BLE_OUTDOOR_MAC);
      if (!scan->start(BLE_SCAN_MS, false)) {
        ESP_LOGW(TAG, "scan start FAIL");
        finishPoll();
        break;
      }
      enterPhase(Phase::Scanning);
      break;
    }

    case Phase::Scanning: {
      NimBLEScan* scan = NimBLEDevice::getScan();
      const bool still = scan && scan->isScanning();
      const bool timedOut = (now - s_phaseMs) > (BLE_SCAN_MS + 2000);
      if (scanComplete() || !still || timedOut) {
        if (timedOut && still) {
          ESP_LOGW(TAG, "scan TIMEOUT — stop");
          scan->stop();
        }
        finishPoll();
      }
      break;
    }

    case Phase::Cleanup:
      bleDeinitSafe();
      esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
      uiLvglSetRgbLowBandwidth(false);
      ESP_LOGI(TAG, "poll END room=%d out=%d",
               (int)s_room.hitThisScan, (int)s_outdoor.hitThisScan);
      enterPhase(Phase::Idle);
      break;
  }

  if (s_phase != Phase::Idle && (now - s_lastPollMs) > BLE_POLL_WATCHDOG_MS) {
    ESP_LOGE(TAG, "poll WATCHDOG — force stop phase=%d", (int)s_phase);
    bleDeinitSafe();
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
    uiLvglSetRgbLowBandwidth(false);
    enterPhase(Phase::Idle);
  }

  // periodicky expirace i mimo poll
  if (s_phase == Phase::Idle && (now % 5000UL) < 200UL) {
    applyToUi();
  }
#endif
}

bool climateBleIsOk(void) {
#if LG_THERMA_BLE_ROOM
  return s_room.ok;
#else
  return false;
#endif
}

bool climateBleIsBusy(void) {
#if LG_THERMA_BLE_ROOM
  return s_phase != Phase::Idle;
#else
  return false;
#endif
}

void climateBleReleaseForTls(void) {
#if LG_THERMA_BLE_ROOM
  if (s_phase != Phase::Idle) {
    return;
  }
  if (s_nimbleUp) {
    bleDeinitSafe();
  }
#endif
}

bool climateBleBootPollPending(void) {
#if LG_THERMA_BLE_ROOM
  if (!s_room.configured && !s_outdoor.configured) {
    return false;
  }
  return s_firstPollPending;
#else
  return false;
#endif
}

float climateBleTempC(void) {
#if LG_THERMA_BLE_ROOM
  return s_room.temp;
#else
  return NAN;
#endif
}

float climateBleHumidity(void) {
#if LG_THERMA_BLE_ROOM
  return s_room.hum;
#else
  return NAN;
#endif
}

int climateBleBatteryPct(void) {
#if LG_THERMA_BLE_ROOM
  return s_room.batt;
#else
  return -1;
#endif
}

int climateBleRssi(void) {
#if LG_THERMA_BLE_ROOM
  return s_room.rssi;
#else
  return 0;
#endif
}

bool climateBleOutdoorIsOk(void) {
#if LG_THERMA_BLE_ROOM
  return s_outdoor.ok;
#else
  return false;
#endif
}

float climateBleOutdoorTempC(void) {
#if LG_THERMA_BLE_ROOM
  return s_outdoor.temp;
#else
  return NAN;
#endif
}

float climateBleOutdoorHumidity(void) {
#if LG_THERMA_BLE_ROOM
  return s_outdoor.hum;
#else
  return NAN;
#endif
}

int climateBleOutdoorBatteryPct(void) {
#if LG_THERMA_BLE_ROOM
  return s_outdoor.batt;
#else
  return -1;
#endif
}

int climateBleOutdoorRssi(void) {
#if LG_THERMA_BLE_ROOM
  return s_outdoor.rssi;
#else
  return 0;
#endif
}

#endif  /* LG_HAS_NATIVE_BLE */
