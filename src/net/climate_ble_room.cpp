// climate_ble_room.cpp — SwitchBot: Wi‑Fi OFF → BLE scan (neblokující FSM) → Wi‑Fi ON
#include "climate_ble_room.h"

#include "ble_config.h"
#include "net_mqtt_client.h"
#include "net_wifi_mgr.h"
#include "ui_eez_model.h"
#include "ui_ui_lvgl.h"

#include <Arduino.h>
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

enum class Phase : uint8_t {
  Idle = 0,
  WifiOffSettle,
  BleInit,
  Scanning,
  Cleanup,
  WifiResume,
};

uint8_t s_mac[6];
bool s_macOk = false;
bool s_ok = false;
float s_temp = NAN;
float s_hum = NAN;
int s_batt = -1;
int s_rssi = 0;
uint32_t s_lastPollMs = 0;
uint32_t s_lastOkMs = 0;
uint32_t s_phaseMs = 0;
Phase s_phase = Phase::Idle;
bool s_scanHit = false;
bool s_nimbleUp = false;
bool s_forcePoll = false;
bool s_firstPollPending = true;

bool parseMac(const char* s, uint8_t out[6]) {
  unsigned v[6];
  if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &v[0], &v[1], &v[2], &v[3],
             &v[4], &v[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; ++i) {
    out[i] = (uint8_t)v[i];
  }
  return true;
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

bool parseFd3dService(const uint8_t* p, size_t n, int rssi) {
  // SwitchBotAPI-BLE meter.md (New Broadcast):
  //  p[0]=type  p[1]=status  p[2]=battery
  //  p[3]=temp decimals(low nibble)  p[4]=sign(bit7=above0)+int  p[5]=humidity
  if (!p || n < 6) {
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
  s_batt = batt;
  s_temp = t;
  s_hum = hum;
  s_rssi = rssi;
  s_lastOkMs = millis();
  return true;
}

bool parseMfr0969(const uint8_t* p, size_t n, int rssi) {
  // Tvůj paket len=13: 6909 | MAC(6) | ?? ?? | temp_dec temp_int hum
  //  6909EC6F03861E6B 78 03 09 97 3E  → T=23.9 H=62 (byty 10,11,12)
  if (!p || n < 11) {
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
    s_temp = t;
    s_hum = hum;
    s_rssi = rssi;
    s_lastOkMs = millis();
    return true;
  };

  // Indoor/Outdoor / novější Meter: temp za MAC+2
  if (tryTemp(10, 11, 12)) {
    return true;
  }
  // Starší layout (pySwitchbot): hned za MAC
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

bool parseAdv(const uint8_t* adv, size_t len, int rssi) {
  if (!adv || len < 4) {
    return false;
  }
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
        if (parseFd3dService(data + 2, dataLen - 2, rssi)) {
          got = true;
        }
      }
    }
    if (fieldType == 0xFF && dataLen >= 11) {
      const uint16_t cid = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
      if (cid == kSbCompanyId) {
        logHex("mfr 0969", data, dataLen);
        if (parseMfr0969(data, dataLen, rssi)) {
          got = true;
        }
      }
    }
    i += fieldLen + 1;
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
  // BLE native je little-endian → tiskneme „běžné“ pořadí
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X", le[5], le[4], le[3],
           le[2], le[1], le[0]);
}

class ScanCbs : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (!d || s_scanHit) {
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

    char macStr[20];
    formatMac(ba->val, macStr, sizeof(macStr));
    const bool matched = s_macOk && macMatch(ba->val, s_mac);
    const bool parsed =
        parseAdv(payload.data(), payload.size(), d->getRSSI());

    ESP_LOGI(TAG, "SB cand %s rssi=%d parsed=%d match=%d T=%.1f", macStr,
             d->getRSSI(), (int)parsed, (int)matched,
             parsed ? (double)s_temp : -999.0);

#if BLE_ACCEPT_ANY_SWITCHBOT
    const bool accept = parsed;
#else
    const bool accept = parsed && matched;
#endif
    if (!accept) {
      return;
    }

    s_scanHit = true;
    ESP_LOGI(TAG, "hit MAC=%s T=%.1f H=%.1f B=%d R=%d", macStr, (double)s_temp,
             (double)s_hum, s_batt, s_rssi);
    NimBLEScan* sc = NimBLEDevice::getScan();
    if (sc && sc->isScanning()) {
      sc->stop();
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    ESP_LOGI(TAG, "scan end reason=%d count=%d hit=%d", reason,
             results.getCount(), (int)s_scanHit);
  }
};

ScanCbs s_scanCbs;

void applyToUi(bool ok) {
  if (ok && !isnan(s_temp)) {
    s_ok = true;
    uiEez.sig_ble = true;
    uiEez.teplota_vnitrni = s_temp;
  }
}

void enterPhase(Phase p) {
  s_phase = p;
  s_phaseMs = millis();
}

void finishPoll(bool ok) {
  if (ok) {
    applyToUi(true);
  } else {
    ESP_LOGW(TAG, "poll MISS");
    // Po 5 min bez úspěchu → offline na hlavní obrazovce
    if (!s_lastOkMs || (millis() - s_lastOkMs) > (5 * 60 * 1000UL)) {
      s_ok = false;
      uiEez.sig_ble = false;
      uiEez.teplota_vnitrni = UI_TEPLOTA_NEPLATNA;
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
  // NEdeinit — opakovaný NimBLEDevice::init po WIFI_OFF shazuje ipc0 stack canary
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
  s_macOk = parseMac(BLE_METER_MAC, s_mac);
  s_lastPollMs = 0;
  s_phase = Phase::Idle;
  s_forcePoll = false;
  s_firstPollPending = true;
  uiEez.teplota_vnitrni = UI_TEPLOTA_NEPLATNA;
  uiEez.sig_ble = false;
  ESP_LOGI(TAG, "init mac_ok=%d MAC=%s interval=%lus (FSM)",
           (int)s_macOk, BLE_METER_MAC,
           (unsigned long)(BLE_POLL_INTERVAL_MS / 1000));
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
  if (s_ok && !isnan(s_temp)) {
    if (s_batt >= 0) {
      snprintf(buf, buflen, "OK  %.1f C  H=%.0f  bat %d%%", (double)s_temp,
               (double)s_hum, s_batt);
    } else {
      snprintf(buf, buflen, "OK  %.1f C  H=%.0f", (double)s_temp,
               (double)s_hum);
    }
    return;
  }
  snprintf(buf, buflen, "Offline - MAC %s", BLE_METER_MAC);
#else
  snprintf(buf, buflen, "BLE vypnuto");
#endif
}

void climateBleTick(void) {
#if LG_THERMA_BLE_ROOM
  if (!s_macOk) {
    return;
  }

  const uint32_t now = millis();

  switch (s_phase) {
    case Phase::Idle: {
      if (netWifiIsBusy() || netWifiIsSuspendedForBle()) {
        return;
      }
      if (!netWifiIsConnected()) {
        return;
      }
      if (netMqttIsBusy()) {
        return;
      }
      // Po restartu nenačítej T hned s Wi-Fi: nejdřív MQTT session, pak první scan.
      if (s_firstPollPending && !s_forcePoll) {
        if (netMqttIsEnabled() && !netMqttIsConnected()) {
          return;
        }
      }
      const bool due = s_forcePoll || s_firstPollPending ||
                       (s_lastPollMs != 0 &&
                        (now - s_lastPollMs >= BLE_POLL_INTERVAL_MS));
      if (!due) {
        return;
      }
      const bool first = s_firstPollPending;
      s_firstPollPending = false;
      s_forcePoll = false;
      s_lastPollMs = now;
      s_scanHit = false;
      ESP_LOGI(TAG, "poll BEGIN%s — MQTT down, WiFi OFF",
               first ? " (first after MQTT)" : "");
      uiLvglSetFrozen(true);
      uiLvglSetRgbLowBandwidth(true);
      // Nejdřív MQTT, ať TLS nepadá uprostřed WIFI_OFF
      netMqttDisconnectQuiet();
      if (!netWifiSuspendForBle()) {
        ESP_LOGW(TAG, "suspend fail");
        uiLvglSetRgbLowBandwidth(false);
        uiLvglSetFrozen(false);
        if (first) {
          s_firstPollPending = true;
          s_lastPollMs = 0;
        }
        return;
      }
      enterPhase(Phase::WifiOffSettle);
      break;
    }

    case Phase::WifiOffSettle:
      // Po WIFI_OFF + TLS teardown: delší settle (ipc0 / BT controller)
      if (now - s_phaseMs < 1500) {
        break;
      }
      enterPhase(Phase::BleInit);
      break;

    case Phase::BleInit: {
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
      scan->setDuplicateFilter(true);
      ESP_LOGI(TAG, "scan start %d ms (any_sb=%d mac=%s)", BLE_SCAN_MS,
               BLE_ACCEPT_ANY_SWITCHBOT, BLE_METER_MAC);
      if (!scan->start(BLE_SCAN_MS, false)) {
        ESP_LOGW(TAG, "scan start FAIL");
        finishPoll(false);
        break;
      }
      enterPhase(Phase::Scanning);
      break;
    }

    case Phase::Scanning: {
      NimBLEScan* scan = NimBLEDevice::getScan();
      const bool still = scan && scan->isScanning();
      const bool timedOut = (now - s_phaseMs) > (BLE_SCAN_MS + 2000);
      if (s_scanHit || !still || timedOut) {
        if (timedOut && still) {
          ESP_LOGW(TAG, "scan TIMEOUT — stop");
          scan->stop();
        }
        finishPoll(s_scanHit);
      }
      break;
    }

    case Phase::Cleanup:
      bleStopScanOnly();
      enterPhase(Phase::WifiResume);
      break;

    case Phase::WifiResume:
      ESP_LOGI(TAG, "poll END ok=%d — WiFi ON", (int)s_scanHit);
      netWifiResumeAfterBle();
      uiLvglSetRgbLowBandwidth(false);
      uiLvglSetFrozen(false);
      enterPhase(Phase::Idle);
      break;
  }

  // Hard safety: uvíznutí → vždy resume
  if (s_phase != Phase::Idle && (now - s_lastPollMs) > BLE_POLL_WATCHDOG_MS) {
    ESP_LOGE(TAG, "poll WATCHDOG — force resume phase=%d", (int)s_phase);
    bleDeinitSafe();
    if (netWifiIsSuspendedForBle()) {
      netWifiResumeAfterBle();
    }
    uiLvglSetRgbLowBandwidth(false);
    uiLvglSetFrozen(false);
    enterPhase(Phase::Idle);
  }
#endif
}

bool climateBleIsOk(void) {
#if LG_THERMA_BLE_ROOM
  return s_ok;
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

float climateBleTempC(void) {
#if LG_THERMA_BLE_ROOM
  return s_temp;
#else
  return NAN;
#endif
}

float climateBleHumidity(void) {
#if LG_THERMA_BLE_ROOM
  return s_hum;
#else
  return NAN;
#endif
}

int climateBleBatteryPct(void) {
#if LG_THERMA_BLE_ROOM
  return s_batt;
#else
  return -1;
#endif
}

int climateBleRssi(void) {
#if LG_THERMA_BLE_ROOM
  return s_rssi;
#else
  return 0;
#endif
}
