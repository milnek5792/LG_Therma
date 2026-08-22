#include "net_wifi_mgr.h"

#include "climate_ble_room.h"
#include "storage_config_nvs.h"
#include "wifi_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

namespace {

enum class WifiPhase : uint8_t {
  kOff = 0,
  kIdle,
  kArmConnect,   // počkej na UI, pak teprve WiFi.begin
  kConnecting,
  kConnected,
  kFailed,
};

bool s_enabled = false;
bool s_connected = false;
bool s_bleSuspended = false;
WifiPhase s_phase = WifiPhase::kOff;
unsigned long s_phaseStartedMs = 0;

char s_status[40] = "Odpojeno";
char s_ssid[33] = "—";
char s_ip[16] = "—";
char s_credSsid[33] = "";
char s_credPass[65] = "";

constexpr unsigned long kConnectTimeoutMs = 30000;
constexpr unsigned long kArmSettleMs = 300;
constexpr unsigned long kConnectGraceMs = 800;  // ignoruj stale WL_CONNECTED po begin
constexpr unsigned long kRetryAfterFailMs = 15000;
constexpr unsigned long kRetryAfterIdleMs = 5000;
constexpr unsigned long kLinkLossDebounceMs = 8000;
unsigned long s_lastRetryMs = 0;
unsigned long s_linkUnhealthySinceMs = 0;

void setStatus(const char* text) {
  strncpy(s_status, text ? text : "", sizeof(s_status));
  s_status[sizeof(s_status) - 1] = '\0';
}

void clearRuntimeNetworkInfo() {
  s_connected = false;
  strncpy(s_ssid, "—", sizeof(s_ssid));
  strncpy(s_ip, "—", sizeof(s_ip));
}

bool loadCredentials() {
  if (storageLoadWifiCredentials(s_credSsid, sizeof(s_credSsid), s_credPass, sizeof(s_credPass))) {
    return s_credSsid[0] != '\0';
  }

  if (WIFI_SSID[0] != '\0' && strcmp(WIFI_SSID, "Vase_Sit") != 0) {
    strncpy(s_credSsid, WIFI_SSID, sizeof(s_credSsid) - 1);
    s_credSsid[sizeof(s_credSsid) - 1] = '\0';
    strncpy(s_credPass, WIFI_PASSWORD, sizeof(s_credPass) - 1);
    s_credPass[sizeof(s_credPass) - 1] = '\0';
    return true;
  }

  s_credSsid[0] = '\0';
  s_credPass[0] = '\0';
  return false;
}

bool hasValidIp() {
  const IPAddress ip = WiFi.localIP();
  return ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0;
}

void refreshConnectedState() {
  if (WiFi.status() != WL_CONNECTED || !hasValidIp()) {
    return;
  }

  s_connected = true;
  s_phase = WifiPhase::kConnected;
  setStatus("Pripojeno");

  const String currentSsid = WiFi.SSID();
  if (currentSsid.length() > 0) {
    strncpy(s_ssid, currentSsid.c_str(), sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
  }

  const String ip = WiFi.localIP().toString();
  if (ip.length() > 0) {
    strncpy(s_ip, ip.c_str(), sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
  }
}

void armConnect() {
  if (!s_enabled) {
    setStatus("Zapnete Wi-Fi");
    return;
  }
  if (!loadCredentials()) {
    setStatus("Nastavte sit v menu");
    Serial.println("[NET] Wi-Fi: nastavte SSID v Nastaveni -> Nastavit sit");
    s_phase = WifiPhase::kFailed;
    return;
  }

  setStatus("Pripojovani...");
  strncpy(s_ssid, s_credSsid, sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  strncpy(s_ip, "—", sizeof(s_ip));
  s_connected = false;
  s_phase = WifiPhase::kArmConnect;
  s_phaseStartedMs = millis();
  Serial.printf("[NET] Wi-Fi priprava '%s'\n", s_credSsid);
}

void beginConnectNow() {
  WiFi.persistent(false);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);  // méně RF contention než PS_NONE
  WiFi.mode(WIFI_STA);

  // Link už drží — ne disconnect+begin (jinak bliká signálka bez důvodu)
  if (WiFi.status() == WL_CONNECTED && hasValidIp()) {
    const String currentSsid = WiFi.SSID();
    if (currentSsid.length() > 0 && currentSsid.equals(s_credSsid)) {
      refreshConnectedState();
      Serial.printf("[NET] Wi-Fi already up %s / %s\n", s_ssid, s_ip);
      return;
    }
  }

  if (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_IDLE_STATUS) {
    WiFi.disconnect(false);
  }

  Serial.printf("[NET] Wi-Fi begin '%s'\n", s_credSsid);
  WiFi.begin(s_credSsid, s_credPass);
  // Nižší TX = méně RF/PSRAM contention s RGB DMA
  WiFi.setTxPower(WIFI_POWER_11dBm);
  s_connected = false;
  s_phase = WifiPhase::kConnecting;
  s_phaseStartedMs = millis();
  s_linkUnhealthySinceMs = 0;
  setStatus("Pripojovani...");
}

void checkConnectFailures() {
  const wl_status_t st = WiFi.status();
  if (st == WL_NO_SSID_AVAIL) {
    s_phase = WifiPhase::kFailed;
    s_connected = false;
    setStatus("Sit nenalezena");
    Serial.println("[NET] Wi-Fi: SSID nenalezeno");
  } else if (st == WL_CONNECT_FAILED) {
    s_phase = WifiPhase::kFailed;
    s_connected = false;
    setStatus("Spatne heslo");
    Serial.println("[NET] Wi-Fi: pripojeni selhalo");
  }
}

}  // namespace

void netWifiInit() {
  storageInit();

  const bool hasCreds = loadCredentials();
  s_enabled = storageLoadWifiEnabled();
  // Po flashi / prázdné NVS: když jsou credentials, Wi‑Fi zapni automaticky
  if (!s_enabled && hasCreds) {
    s_enabled = true;
    storageSaveWifiEnabled(true);
    Serial.println("[NET] Wi-Fi auto-enable (creds v NVS)");
  }

  if (s_enabled) {
    WiFi.persistent(false);
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    WiFi.mode(WIFI_STA);
    setStatus("Pripraveno");
    s_phase = WifiPhase::kIdle;
    s_lastRetryMs = 0;

    if (hasCreds) {
      armConnect();
    } else {
      setStatus("Nastavte sit v menu");
      s_phase = WifiPhase::kFailed;
    }
  } else {
    WiFi.mode(WIFI_OFF);
    setStatus("Vypnuto");
    s_phase = WifiPhase::kOff;
    clearRuntimeNetworkInfo();
  }

  Serial.printf("[NET] Wi-Fi init enabled=%d creds=%d phase=%d\n",
                (int)s_enabled, (int)hasCreds, (int)s_phase);
}

void netWifiSetEnabled(bool on) {
  s_enabled = on;
  storageSaveWifiEnabled(on);

  if (!on) {
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
    s_phase = WifiPhase::kOff;
    setStatus("Vypnuto");
    clearRuntimeNetworkInfo();
    Serial.println("[NET] Wi-Fi vypnuto");
    return;
  }

  WiFi.persistent(false);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);  // méně RF contention než PS_NONE
  WiFi.mode(WIFI_STA);
  setStatus("Pripraveno");
  s_phase = WifiPhase::kIdle;
  s_connected = false;
  Serial.println("[NET] Wi-Fi zapnuto");
}

bool netWifiIsEnabled() {
  return s_enabled;
}

void netWifiConnect() {
  if (!s_enabled) {
    netWifiSetEnabled(true);
  }
  armConnect();
}

bool netWifiIsConnected() {
  // Jen cached stav z ticku — nevolat WiFi.status()/localIP každý LVGL frame
  return s_phase == WifiPhase::kConnected && s_connected;
}

bool netWifiIsBusy(void) {
  return s_phase == WifiPhase::kArmConnect || s_phase == WifiPhase::kConnecting;
}

bool netWifiIsSuspendedForBle(void) { return s_bleSuspended; }

bool netWifiSuspendForBle(void) {
  if (s_bleSuspended) {
    return true;
  }
  Serial.println("[NET] Wi-Fi SUSPEND for BLE (WIFI_OFF)");
  s_bleSuspended = true;
  s_connected = false;
  setStatus("BLE scan...");
  WiFi.disconnect(false);
  delay(50);
  WiFi.mode(WIFI_OFF);
  return true;
}

bool netWifiResumeAfterBle(void) {
  if (!s_bleSuspended) {
    return true;
  }
  Serial.println("[NET] Wi-Fi RESUME after BLE");
  s_bleSuspended = false;
  if (!s_enabled) {
    setStatus("Vypnuto");
    return true;
  }
  WiFi.persistent(false);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  WiFi.mode(WIFI_STA);
  setStatus("Obnovuji Wi-Fi...");
  armConnect();
  return true;
}

bool netWifiHasCredentials() {
  return loadCredentials();
}

void netWifiSetCredentials(const char* ssid, const char* pass) {
  if (!ssid || !pass) {
    return;
  }
  strncpy(s_credSsid, ssid, sizeof(s_credSsid) - 1);
  s_credSsid[sizeof(s_credSsid) - 1] = '\0';
  strncpy(s_credPass, pass, sizeof(s_credPass) - 1);
  s_credPass[sizeof(s_credPass) - 1] = '\0';
  storageSaveWifiCredentials(s_credSsid, s_credPass);
}

const char* netWifiStatus() {
  return s_status;
}

const char* netWifiSsid() {
  return s_ssid;
}

const char* netWifiIp() {
  return s_ip;
}

void netWifiTick() {
  if (!s_enabled || s_bleSuspended) {
    return;
  }

  if (s_phase == WifiPhase::kArmConnect) {
    if (millis() - s_phaseStartedMs < kArmSettleMs) {
      return;
    }
    beginConnectNow();
    return;
  }

  if (s_phase == WifiPhase::kConnecting) {
    if (millis() - s_phaseStartedMs >= kConnectTimeoutMs) {
      s_phase = WifiPhase::kFailed;
      s_connected = false;
      setStatus("Timeout pripojeni");
      Serial.println("[NET] Wi-Fi timeout");
      WiFi.disconnect(false);
      return;
    }

    // Po begin ignoruj krátce starý WL_CONNECTED (jinak falešné „OK“)
    if (millis() - s_phaseStartedMs < kConnectGraceMs) {
      checkConnectFailures();
      return;
    }

    if (WiFi.status() == WL_CONNECTED && hasValidIp()) {
      refreshConnectedState();
      Serial.printf("[NET] Wi-Fi OK: %s / %s\n", s_ssid, s_ip);
      return;
    }

    checkConnectFailures();
    return;
  }

  // kConnected — udržet link; krátké výpadky (modem sleep / BLE coexistence)
  if (s_phase == WifiPhase::kConnected) {
    if (WiFi.status() == WL_CONNECTED && hasValidIp()) {
      s_linkUnhealthySinceMs = 0;
      if (!s_connected) {
        refreshConnectedState();
      }
      return;
    }

    // BLE scan může krátce rozhodit RF — neodpojovat hned
    if (climateBleIsBusy()) {
      s_linkUnhealthySinceMs = 0;
      return;
    }

    const uint32_t now = millis();
    if (s_linkUnhealthySinceMs == 0) {
      s_linkUnhealthySinceMs = now;
      Serial.printf("[NET] Wi-Fi link hiccup — debounce %lu ms\n",
                    (unsigned long)kLinkLossDebounceMs);
      return;
    }
    if (now - s_linkUnhealthySinceMs < kLinkLossDebounceMs) {
      return;
    }
    s_linkUnhealthySinceMs = 0;

    s_connected = false;
    s_phase = WifiPhase::kIdle;
    setStatus("Odpojeno");
    clearRuntimeNetworkInfo();
    s_lastRetryMs = millis();
    Serial.println("[NET] Wi-Fi ztraceno — retry brzy");
    return;
  }

  if (s_connected) {
    s_connected = false;
    clearRuntimeNetworkInfo();
  }

  // Auto-reconnect po fail / idle (jinak po rebootu / výpadku zůstane viset)
  if (s_phase == WifiPhase::kFailed || s_phase == WifiPhase::kIdle) {
    if (!loadCredentials()) {
      return;
    }
    const unsigned long waitMs =
        (s_phase == WifiPhase::kFailed) ? kRetryAfterFailMs : kRetryAfterIdleMs;
    if (s_lastRetryMs == 0) {
      s_lastRetryMs = millis();
      return;
    }
    if (millis() - s_lastRetryMs < waitMs) {
      return;
    }
    s_lastRetryMs = millis();

    if (WiFi.status() == WL_CONNECTED && hasValidIp()) {
      refreshConnectedState();
      Serial.println("[NET] Wi-Fi soft-recover (link drzen)");
      return;
    }

    Serial.printf("[NET] Wi-Fi auto-retry (phase=%d)\n", (int)s_phase);
    armConnect();
  }
}
