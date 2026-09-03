#include "storage_config_nvs.h"

#include "climate_plan.h"
#include "climate_regulator.h"
#include "h2_uart_protocol.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace {

Preferences s_prefs;
bool s_open = false;
SemaphoreHandle_t s_nvsMux = nullptr;
bool s_tcSessionDirty = false;
bool s_tcSessionOnPending = false;
uint8_t s_tcSessionSpPending = 35;

constexpr const char* kNs = "lg_therma";
constexpr const char* kKeyWifiEn = "wifi_en";
constexpr const char* kKeyWifiSsid = "wifi_ssid";
constexpr const char* kKeyWifiPass = "wifi_pass";
constexpr const char* kKeyMqttEn = "mqtt_en";
constexpr const char* kKeyBlPct = "bl_pct";
constexpr const char* kKeyBlSleep = "bl_sleep";
constexpr const char* kKeyPlan = "plan_cfg";
constexpr const char* kKeyReg = "reg_cfg";
constexpr const char* kKeyUiRezim = "ui_rezim";
constexpr const char* kKeyTcOn = "tc_on";
constexpr const char* kKeyTcSp = "tc_sp";
constexpr const char* kKeyBleRoomMac = "ble_room";
constexpr const char* kKeyBleOutMac = "ble_out";
constexpr const char* kKeyEnMeta = "en_meta";
constexpr const char* kKeyEnPwr0 = "en_p0";
constexpr const char* kKeyEnPwr1 = "en_p1";
constexpr const char* kKeyEnPwr2 = "en_p2";
constexpr const char* kKeyEnPwr3 = "en_p3";
constexpr const char* kKeyEnPwr4 = "en_p4";
constexpr const char* kKeyEnPwr5 = "en_p5";
constexpr const char* kKeyEnPwr6 = "en_p6";
constexpr uint32_t kPlanMagic = 0x504C414Eu;
/** Bump jen při změně layoutu PlanTydenConfig — ne při změně výchozích hodnot. */
constexpr uint16_t kPlanVersion = 4;
/** Nejstarší verze se stejným binárním layoutem (včetně cas_rezim). */
constexpr uint16_t kPlanVersionMinCompat = 2;
constexpr uint32_t kRegMagic = 0x52454731u;  // REG1
constexpr uint16_t kRegVersion = 5;
constexpr uint32_t kSleepOptsSec[] = {0, 60, 120, 300, 600, 1800};

class NvsLock {
 public:
  NvsLock() : taken_(s_nvsMux && xSemaphoreTake(s_nvsMux, portMAX_DELAY) == pdTRUE) {}
  ~NvsLock() {
    if (taken_) {
      xSemaphoreGive(s_nvsMux);
    }
  }

 private:
  bool taken_;
};

bool isValidSleepTimeoutSec(uint32_t sec) {
  for (uint32_t v : kSleepOptsSec) {
    if (v == sec) {
      return true;
    }
  }
  return false;
}

/** NVS v4 — venkovní body + bias_pct (nepoužito v regulaci). */
struct RegulatorConfigV4 {
  float room_sp_c;
  float t_out_cold_c;
  float u_cold_pct;
  float t_out_warm_c;
  float u_warm_pct;
  float kp;
  float ki;
  float kd;
  float bias_pct;
  float trim_limit_pct;
  float deadband_c;
  uint8_t use_equitherm;
  uint8_t _pad[3];
};

void migrateRegulatorV4ToV5(const RegulatorConfigV4* old, RegulatorConfig* cfg) {
  if (!old || !cfg) {
    return;
  }
  cfg->room_sp_c = old->room_sp_c;
  cfg->t_water_cold_c = REG_EQ_WATER_COLD_DEFAULT_C;
  cfg->t_water_warm_c = REG_EQ_WATER_WARM_DEFAULT_C;
  cfg->offset_c = 0.0f;
  cfg->kp = old->kp;
  cfg->ki = old->ki;
  cfg->kd = old->kd;
  cfg->trim_limit_pct = old->trim_limit_pct;
  cfg->deadband_c = old->deadband_c;
  cfg->use_equitherm = old->use_equitherm;
  cfg->_pad[0] = cfg->_pad[1] = cfg->_pad[2] = 0;
}

void ensureOpen() {
  if (s_open) {
    return;
  }
  if (!s_prefs.begin(kNs, false)) {
    Serial.println("[NVS] begin(lg_therma) FAIL");
    return;
  }
  s_open = true;
}

}  // namespace

void storageInit() {
  if (!s_nvsMux) {
    s_nvsMux = xSemaphoreCreateMutex();
  }
  NvsLock lock;
  ensureOpen();
}

bool storageLoadWifiEnabled() {
  NvsLock lock;
  ensureOpen();
  return s_prefs.getBool(kKeyWifiEn, false);
}

bool storageWifiEnabledIsSet() {
  NvsLock lock;
  ensureOpen();
  return s_prefs.isKey(kKeyWifiEn);
}

void storageSaveWifiEnabled(bool on) {
  NvsLock lock;
  ensureOpen();
  s_prefs.putBool(kKeyWifiEn, on);
}

bool storageLoadWifiCredentials(char* ssid, size_t ssidLen, char* pass, size_t passLen) {
  if (!ssid || ssidLen == 0 || !pass || passLen == 0) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  String storedSsid = s_prefs.getString(kKeyWifiSsid, "");
  String storedPass = s_prefs.getString(kKeyWifiPass, "");
  if (storedSsid.length() == 0) {
    ssid[0] = '\0';
    pass[0] = '\0';
    return false;
  }
  strncpy(ssid, storedSsid.c_str(), ssidLen - 1);
  ssid[ssidLen - 1] = '\0';
  strncpy(pass, storedPass.c_str(), passLen - 1);
  pass[passLen - 1] = '\0';
  return true;
}

void storageSaveWifiCredentials(const char* ssid, const char* pass) {
  if (!ssid || !pass) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  s_prefs.putString(kKeyWifiSsid, ssid);
  s_prefs.putString(kKeyWifiPass, pass);
}

bool storageLoadMqttEnabled() {
  NvsLock lock;
  ensureOpen();
  return s_prefs.getBool(kKeyMqttEn, false);
}

void storageSaveMqttEnabled(bool on) {
  NvsLock lock;
  ensureOpen();
  s_prefs.putBool(kKeyMqttEn, on);
}

uint8_t storageLoadBrightness(void) {
  NvsLock lock;
  ensureOpen();
  if (!s_open) {
    return 60;
  }
  int v = 60;
  if (s_prefs.isKey(kKeyBlPct)) {
    v = s_prefs.getInt(kKeyBlPct, -1);
    if (v < 0) {
      v = static_cast<int>(s_prefs.getUInt(kKeyBlPct, 60));
    }
  }
  if (v < 10) {
    v = 10;
  }
  if (v > 97) {
    v = 97;
  }
  Serial.printf("[NVS] load jas=%d\n", v);
  return static_cast<uint8_t>(v);
}

void storageSaveBrightness(uint8_t percent) {
  NvsLock lock;
  ensureOpen();
  if (!s_open) {
    return;
  }
  if (percent < 10) {
    percent = 10;
  }
  if (percent > 97) {
    percent = 97;
  }
  // TYPE_MISMATCH (starý typ klíče) → smazat a zapsat znovu
  size_t n = s_prefs.putInt(kKeyBlPct, percent);
  if (n == 0) {
    s_prefs.remove(kKeyBlPct);
    n = s_prefs.putInt(kKeyBlPct, percent);
  }
  Serial.printf("[NVS] save jas=%u → %s\n", (unsigned)percent, n ? "ok" : "FAIL");
}

uint32_t storageLoadSleepTimeoutSec(void) {
  NvsLock lock;
  ensureOpen();
  if (!s_open) {
    return 120;
  }
  uint32_t sec = 120;
  if (s_prefs.isKey(kKeyBlSleep)) {
    sec = s_prefs.getUInt(kKeyBlSleep, UINT32_MAX);
    if (!isValidSleepTimeoutSec(sec)) {
      const int alt = s_prefs.getInt(kKeyBlSleep, -1);
      if (alt >= 0 && isValidSleepTimeoutSec(static_cast<uint32_t>(alt))) {
        sec = static_cast<uint32_t>(alt);
      } else {
        sec = 120;
      }
    }
  }
  Serial.printf("[NVS] load usinani=%lus\n", (unsigned long)sec);
  return sec;
}

void storageSaveSleepTimeoutSec(uint32_t sec) {
  NvsLock lock;
  ensureOpen();
  if (!s_open) {
    return;
  }
  if (!isValidSleepTimeoutSec(sec)) {
    sec = 120;
  }
  size_t n = s_prefs.putUInt(kKeyBlSleep, sec);
  if (n == 0) {
    s_prefs.remove(kKeyBlSleep);
    n = s_prefs.putUInt(kKeyBlSleep, sec);
  }
  Serial.printf("[NVS] save usinani=%lu → %s\n", (unsigned long)sec, n ? "ok" : "FAIL");
}

bool storageLoadPlanConfig(PlanTydenConfig* cfg) {
  if (!cfg) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  size_t len = s_prefs.getBytesLength(kKeyPlan);
  if (len < sizeof(PlanTydenConfig) + 6) {
    return false;
  }
  uint8_t buf[sizeof(PlanTydenConfig) + 8];
  const size_t got = s_prefs.getBytes(kKeyPlan, buf, sizeof(buf));
  if (got < 6 + sizeof(PlanTydenConfig)) {
    return false;
  }
  uint32_t magic = 0;
  uint16_t version = 0;
  memcpy(&magic, buf, 4);
  memcpy(&version, buf + 4, 2);
  if (magic != kPlanMagic) {
    return false;
  }
  if (version < kPlanVersionMinCompat) {
    return false;
  }
  memcpy(cfg, buf + 6, sizeof(PlanTydenConfig));
  return true;
}

void storageSavePlanConfig(const PlanTydenConfig* cfg) {
  if (!cfg) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  uint8_t buf[6 + sizeof(PlanTydenConfig)];
  memcpy(buf, &kPlanMagic, 4);
  memcpy(buf + 4, &kPlanVersion, 2);
  memcpy(buf + 6, cfg, sizeof(PlanTydenConfig));
  s_prefs.putBytes(kKeyPlan, buf, sizeof(buf));
}

void saveRegulatorConfigLocked(const RegulatorConfig* cfg) {
  if (!cfg || !s_open) {
    return;
  }
  uint8_t buf[6 + sizeof(RegulatorConfig)];
  memcpy(buf, &kRegMagic, 4);
  memcpy(buf + 4, &kRegVersion, 2);
  memcpy(buf + 6, cfg, sizeof(RegulatorConfig));
  s_prefs.putBytes(kKeyReg, buf, sizeof(buf));
}

bool storageLoadRegulatorConfig(RegulatorConfig* cfg) {
  if (!cfg) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  size_t len = s_prefs.getBytesLength(kKeyReg);
  if (len < 6) {
    return false;
  }
  uint8_t buf[128];
  const size_t got = s_prefs.getBytes(kKeyReg, buf, sizeof(buf));
  if (got < 6) {
    return false;
  }
  uint32_t magic = 0;
  uint16_t version = 0;
  memcpy(&magic, buf, 4);
  memcpy(&version, buf + 4, 2);
  if (magic != kRegMagic || version < 1) {
    return false;
  }
  if (version >= kRegVersion) {
    if (got < 6 + sizeof(RegulatorConfig)) {
      return false;
    }
    memcpy(cfg, buf + 6, sizeof(RegulatorConfig));
    return true;
  }
  if (version <= 4) {
    if (got < 6 + sizeof(RegulatorConfigV4)) {
      return false;
    }
    RegulatorConfigV4 old{};
    memcpy(&old, buf + 6, sizeof(RegulatorConfigV4));
    migrateRegulatorV4ToV5(&old, cfg);
    saveRegulatorConfigLocked(cfg);
    return true;
  }
  return false;
}

void storageSaveRegulatorConfig(const RegulatorConfig* cfg) {
  if (!cfg) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  saveRegulatorConfigLocked(cfg);
}

bool storageLoadUiRezim(uint8_t* out) {
  if (!out) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  if (!s_prefs.isKey(kKeyUiRezim)) {
    return false;
  }
  const int v = s_prefs.getInt(kKeyUiRezim, 0);
  if (v != 0 && v != 1) {
    return false;
  }
  *out = (uint8_t)v;
  return true;
}

void storageSaveUiRezim(uint8_t rezim) {
  NvsLock lock;
  ensureOpen();
  s_prefs.putInt(kKeyUiRezim, (int)rezim);
}

bool storageLoadTcSession(bool* outOn, uint8_t* outSp) {
  if (!outOn) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  if (!s_prefs.isKey(kKeyTcOn)) {
    return false;
  }
  *outOn = s_prefs.getBool(kKeyTcOn, false);
  if (outSp) {
    const int sp = s_prefs.getInt(kKeyTcSp, 35);
    if (sp >= 15 && sp <= 65) {
      *outSp = (uint8_t)sp;
    } else {
      *outSp = 35;
    }
  }
  return true;
}

void storageSaveTcSession(bool on, uint8_t spC) {
  NvsLock lock;
  ensureOpen();
  s_prefs.putBool(kKeyTcOn, on);
  if (spC >= 15 && spC <= 65) {
    s_prefs.putInt(kKeyTcSp, (int)spC);
  }
}

void storageRequestSaveTcSession(bool on, uint8_t spC) {
  s_tcSessionOnPending = on;
  if (spC >= 15 && spC <= 65) {
    s_tcSessionSpPending = spC;
  }
  s_tcSessionDirty = true;
}

void storageFlushTcSessionPending(void) {
  if (!s_tcSessionDirty) {
    return;
  }
  s_tcSessionDirty = false;
  storageSaveTcSession(s_tcSessionOnPending, s_tcSessionSpPending);
}

bool storageLoadBleRoomMac(char* mac, size_t len) {
  if (!mac || len < H2_MAC_STR_LEN) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  // isKey dřív než getString — jinak Preferences loguje ESP_LOGE při NOT_FOUND.
  if (!s_prefs.isKey(kKeyBleRoomMac)) {
    mac[0] = '\0';
    return false;
  }
  String s = s_prefs.getString(kKeyBleRoomMac, "");
  if (s.length() != 17) {
    mac[0] = '\0';
    return false;
  }
  strncpy(mac, s.c_str(), len - 1);
  mac[len - 1] = '\0';
  return true;
}

void storageSaveBleRoomMac(const char* mac) {
  NvsLock lock;
  ensureOpen();
  if (!mac || mac[0] == '\0' || strcmp(mac, "00:00:00:00:00:00") == 0) {
    s_prefs.remove(kKeyBleRoomMac);
    return;
  }
  s_prefs.putString(kKeyBleRoomMac, mac);
}

bool storageLoadBleOutdoorMac(char* mac, size_t len) {
  if (!mac || len < H2_MAC_STR_LEN) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  if (!s_prefs.isKey(kKeyBleOutMac)) {
    mac[0] = '\0';
    return false;
  }
  String s = s_prefs.getString(kKeyBleOutMac, "");
  if (s.length() != 17) {
    mac[0] = '\0';
    return false;
  }
  strncpy(mac, s.c_str(), len - 1);
  mac[len - 1] = '\0';
  return true;
}

void storageSaveBleOutdoorMac(const char* mac) {
  NvsLock lock;
  ensureOpen();
  if (!mac || mac[0] == '\0' || strcmp(mac, "00:00:00:00:00:00") == 0) {
    s_prefs.remove(kKeyBleOutMac);
    return;
  }
  s_prefs.putString(kKeyBleOutMac, mac);
}

namespace {

const char* weekPowerKey(int day) {
  static const char* keys[] = {kKeyEnPwr0, kKeyEnPwr1, kKeyEnPwr2, kKeyEnPwr3,
                               kKeyEnPwr4, kKeyEnPwr5, kKeyEnPwr6};
  if (day < 0 || day > 6) {
    return kKeyEnPwr0;
  }
  return keys[day];
}

}  // namespace

bool storageLoadEnergyMeta(void* dst, size_t len) {
  if (!dst || len == 0) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  if (!s_prefs.isKey(kKeyEnMeta)) {
    return false;
  }
  const size_t got = s_prefs.getBytesLength(kKeyEnMeta);
  if (got != len) {
    return false;
  }
  return s_prefs.getBytes(kKeyEnMeta, dst, len) == len;
}

void storageSaveEnergyMeta(const void* src, size_t len) {
  if (!src || len == 0) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  s_prefs.putBytes(kKeyEnMeta, src, len);
}

bool storageLoadEnergyWeekPower(uint16_t* dst, size_t count) {
  if (!dst || count < 7 * 1440) {
    return false;
  }
  NvsLock lock;
  ensureOpen();
  bool any = false;
  for (int d = 0; d < 7; ++d) {
    const char* key = weekPowerKey(d);
    uint16_t* day = dst + d * 1440;
    if (!s_prefs.isKey(key)) {
      memset(day, 0, 1440 * sizeof(uint16_t));
      continue;
    }
    const size_t want = 1440 * sizeof(uint16_t);
    if (s_prefs.getBytesLength(key) != want) {
      memset(day, 0, want);
      continue;
    }
    if (s_prefs.getBytes(key, day, want) == want) {
      any = true;
    }
  }
  return any;
}

void storageSaveEnergyWeekPower(const uint16_t* src, size_t count) {
  if (!src || count < 7 * 1440) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  for (int d = 0; d < 7; ++d) {
    const char* key = weekPowerKey(d);
    s_prefs.putBytes(key, src + d * 1440, 1440 * sizeof(uint16_t));
  }
}

void storageSaveEnergyWeekPowerDay(int dayIndex, const uint16_t* daySamples) {
  if (!daySamples || dayIndex < 0 || dayIndex > 6) {
    return;
  }
  NvsLock lock;
  ensureOpen();
  s_prefs.putBytes(weekPowerKey(dayIndex), daySamples, 1440 * sizeof(uint16_t));
}
