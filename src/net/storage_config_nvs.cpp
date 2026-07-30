#include "storage_config_nvs.h"

#include <Preferences.h>
#include <string.h>

namespace {

Preferences s_prefs;
bool s_open = false;

constexpr const char* kNs = "lg_therma";
constexpr const char* kKeyWifiEn = "wifi_en";
constexpr const char* kKeyWifiSsid = "wifi_ssid";
constexpr const char* kKeyWifiPass = "wifi_pass";
constexpr const char* kKeyMqttEn = "mqtt_en";
constexpr const char* kKeyBlPct = "bl_pct";
constexpr const char* kKeyBlSleep = "bl_sleep";

void ensureOpen() {
  if (!s_open) {
    s_prefs.begin(kNs, false);
    s_open = true;
  }
}

}  // namespace

void storageInit() {
  ensureOpen();
}

bool storageLoadWifiEnabled() {
  ensureOpen();
  return s_prefs.getBool(kKeyWifiEn, false);
}

void storageSaveWifiEnabled(bool on) {
  ensureOpen();
  s_prefs.putBool(kKeyWifiEn, on);
}

bool storageLoadWifiCredentials(char* ssid, size_t ssidLen, char* pass, size_t passLen) {
  if (!ssid || ssidLen == 0 || !pass || passLen == 0) {
    return false;
  }
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
  ensureOpen();
  s_prefs.putString(kKeyWifiSsid, ssid);
  s_prefs.putString(kKeyWifiPass, pass);
}

bool storageLoadMqttEnabled() {
  ensureOpen();
  return s_prefs.getBool(kKeyMqttEn, false);
}

void storageSaveMqttEnabled(bool on) {
  ensureOpen();
  s_prefs.putBool(kKeyMqttEn, on);
}

uint8_t storageLoadBrightness(void) {
  ensureOpen();
  int v = s_prefs.getInt(kKeyBlPct, 60);
  if (v < 10) {
    v = 10;
  }
  if (v > 97) {
    v = 97;
  }
  return static_cast<uint8_t>(v);
}

void storageSaveBrightness(uint8_t percent) {
  ensureOpen();
  if (percent < 10) {
    percent = 10;
  }
  if (percent > 97) {
    percent = 97;
  }
  s_prefs.putInt(kKeyBlPct, percent);
}

uint32_t storageLoadSleepTimeoutSec(void) {
  ensureOpen();
  return static_cast<uint32_t>(s_prefs.getUInt(kKeyBlSleep, 120));
}

void storageSaveSleepTimeoutSec(uint32_t sec) {
  ensureOpen();
  s_prefs.putUInt(kKeyBlSleep, sec);
}
