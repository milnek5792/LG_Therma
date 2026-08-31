// bridge_ota.cpp — Wi‑Fi STA + ArduinoOTA na povel z Tab5 (UART)
#include "bridge_ota.h"

#include "bridge_ota_config.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t kConnectTimeoutMs = 30000;

Preferences s_prefs;
BridgeUartOutFn s_uartOut = nullptr;
bool s_connecting = false;
bool s_wifiOn = false;
bool s_otaStarted = false;
uint32_t s_connectStartMs = 0;
uint32_t s_idleOffAtMs = 0;
char s_hostname[32] = "";

void uartLine(const char* line) {
  if (s_uartOut && line) {
    s_uartOut(line);
  }
}

void touchIdleTimer(void) {
  s_idleOffAtMs = millis() + BRIDGE_OTA_IDLE_MS;
}

void setupOtaCallbacks(void) {
  ArduinoOTA.onStart([]() {
    touchIdleTimer();
    Serial.println("[OTA] upload start");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    (void)progress;
    (void)total;
    touchIdleTimer();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] upload done — restart");
    uartLine("OTA DONE\n");
    delay(200);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    char buf[48];
    snprintf(buf, sizeof(buf), "ERR OTA %u\n", (unsigned)err);
    uartLine(buf);
    Serial.printf("[OTA] error %u\n", (unsigned)err);
  });
}

void beginOtaService(void) {
  if (s_otaStarted) {
    return;
  }
  setupOtaCallbacks();
  ArduinoOTA.setHostname(s_hostname);
  if (BRIDGE_OTA_PASSWORD[0] != '\0') {
    ArduinoOTA.setPassword(BRIDGE_OTA_PASSWORD);
  }
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.begin();
  s_otaStarted = true;

  char buf[96];
  snprintf(buf, sizeof(buf), "OTA READY host=%s ip=%s port=3232\n", s_hostname,
           WiFi.localIP().toString().c_str());
  uartLine(buf);
  Serial.printf("[OTA] ready %s %s\n", s_hostname,
                WiFi.localIP().toString().c_str());
  touchIdleTimer();
}

void saveWifiCreds(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) {
    return;
  }
  s_prefs.putString("wifi_ssid", ssid);
  s_prefs.putString("wifi_pass", pass ? pass : "");
}

bool loadWifiCreds(char* ssid, size_t ssidLen, char* pass, size_t passLen) {
  if (!ssid || ssidLen == 0) {
    return false;
  }
  String s = s_prefs.getString("wifi_ssid", "");
  if (s.length() == 0) {
    return false;
  }
  strncpy(ssid, s.c_str(), ssidLen - 1);
  ssid[ssidLen - 1] = '\0';
  if (pass && passLen > 0) {
    String p = s_prefs.getString("wifi_pass", "");
    strncpy(pass, p.c_str(), passLen - 1);
    pass[passLen - 1] = '\0';
  }
  return ssid[0] != '\0';
}

}  // namespace

void bridgeOtaInit(BridgeUartOutFn uartOut) {
  s_uartOut = uartOut;
  s_prefs.begin("h2_bridge", false);
  const uint32_t macLo = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFull);
  snprintf(s_hostname, sizeof(s_hostname), "%s-%06X", BRIDGE_OTA_HOST_PREFIX,
           (unsigned)macLo);
  Serial.printf("[OTA] init host=%s\n", s_hostname);
}

bool bridgeOtaWifiBusy(void) { return s_connecting || s_wifiOn; }

bool bridgeOtaStartWifi(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0] || s_connecting) {
    return false;
  }
  if (s_wifiOn) {
    bridgeOtaStopWifi();
    delay(100);
  }

  saveWifiCreds(ssid, pass);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.begin(ssid, pass ? pass : "");

  s_connecting = true;
  s_connectStartMs = millis();
  uartLine("WIFI CONNECTING\n");
  Serial.printf("[OTA] WiFi connect ssid=%s\n", ssid);
  return true;
}

bool bridgeOtaStartWifiStored(void) {
  char ssid[33];
  char pass[65];
  if (!loadWifiCreds(ssid, sizeof(ssid), pass, sizeof(pass))) {
    return false;
  }
  return bridgeOtaStartWifi(ssid, pass);
}

void bridgeOtaStopWifi(void) {
  if (s_otaStarted) {
    ArduinoOTA.end();
    s_otaStarted = false;
  }
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  s_wifiOn = false;
  s_connecting = false;
  s_idleOffAtMs = 0;
  uartLine("WIFI OFF\n");
  Serial.println("[OTA] WiFi off");
}

void bridgeOtaTick(void) {
  if (s_connecting) {
    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      s_connecting = false;
      s_wifiOn = true;
      char buf[48];
      snprintf(buf, sizeof(buf), "WIFI IP=%s\n",
               WiFi.localIP().toString().c_str());
      uartLine(buf);
      beginOtaService();
      return;
    }
    if (millis() - s_connectStartMs > kConnectTimeoutMs) {
      s_connecting = false;
      uartLine("ERR WIFI\n");
      bridgeOtaStopWifi();
    }
    return;
  }

  if (!s_wifiOn) {
    return;
  }

  if (s_otaStarted) {
    ArduinoOTA.handle();
  }

  if (s_idleOffAtMs != 0 && (int32_t)(millis() - s_idleOffAtMs) >= 0) {
    Serial.println("[OTA] idle timeout — WiFi off");
    bridgeOtaStopWifi();
  }
}
