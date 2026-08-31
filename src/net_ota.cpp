// net_ota.cpp — ArduinoOTA pro upload z PlatformIO (espota), ne HTTP server
#include "net_ota.h"

#include "ota_config.h"
#include "app_version.h"
#include "net_mqtt_client.h"
#include "net_sdio_arbiter.h"
#include "net_wifi_mgr.h"
#include "ui_display_bus.h"
#include "ui_task_ui.h"
#include "ui_ui_lvgl.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "M5Unified.h"

namespace {

bool s_started = false;
bool s_busy = false;
bool s_uiSuspended = false;
int s_lastPct = -1;
uint32_t s_lastProgressDrawMs = 0;

constexpr int kProgressStepPct = 2;
constexpr uint32_t kProgressMinIntervalMs = 250;

void paintOtaScreen(const char* line1, const char* line2) {
  if (!uiDisplayBusLock(pdMS_TO_TICKS(500))) {
    Serial.printf("[OTA] %s %s\n", line1 ? line1 : "", line2 ? line2 : "");
    return;
  }

  auto& d = M5.Display;
  const int cx = d.width() / 2;
  const int cy = d.height() / 2;

  d.fillScreen(TFT_BLACK);
  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(3);
  if (line1 && line1[0]) {
    d.drawString(line1, cx, cy - 48);
  }
  if (line2 && line2[0]) {
    d.setTextSize(2);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawString(line2, cx, cy + 24);
  }
  d.setTextDatum(top_left);
  d.setTextSize(1);
  uiDisplayBusUnlock();
  Serial.printf("[OTA] %s | %s\n", line1 ? line1 : "", line2 ? line2 : "");
}

void enterOtaUiMode(void) {
  s_busy = true;
  s_lastPct = -1;
  s_lastProgressDrawMs = 0;
  uiLvglSetOtaLock(true);
  netMqttSetEnabled(false);
  netSdioSetTlsBusy(true);
  if (!s_uiSuspended) {
    lgTaskUiSuspend();
    s_uiSuspended = true;
  }
  vTaskDelay(pdMS_TO_TICKS(120));
  paintOtaScreen("OTA upload", APP_FW_VERSION);
}

void leaveOtaUiMode(bool resumeUi) {
  uiLvglSetOtaLock(false);
  s_busy = false;
  s_lastPct = -1;
  s_lastProgressDrawMs = 0;
  netSdioSetTlsBusy(false);
  netSdioClearUiFreeze();
  if (resumeUi && s_uiSuspended) {
    lgTaskUiResume();
    s_uiSuspended = false;
  }
}

void setupCallbacks() {
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] start (PlatformIO espota)");
    enterOtaUiMode();
  });

  ArduinoOTA.onEnd([]() {
    paintOtaScreen("OTA HOTOVO", "restart...");
    delay(800);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) {
      return;
    }
    const int pct = (int)((progress * 100ULL) / total);
    if (pct == s_lastPct) {
      return;
    }
    const uint32_t now = millis();
    const bool done = (pct >= 100);
    const bool enoughPct =
        (s_lastPct < 0) || done || (pct >= s_lastPct + kProgressStepPct);
    const bool enoughTime =
        (s_lastProgressDrawMs == 0) ||
        (now - s_lastProgressDrawMs >= kProgressMinIntervalMs);
    if (!done && !enoughPct && !enoughTime) {
      return;
    }

    s_lastPct = pct;
    s_lastProgressDrawMs = now;
    char l2[48];
    snprintf(l2, sizeof(l2), "%u / %u KB", progress / 1024, total / 1024);
    char l1[32];
    snprintf(l1, sizeof(l1), "OTA %d %%", pct);
    paintOtaScreen(l1, l2);
  });

  ArduinoOTA.onError([](ota_error_t err) {
    char msg[32];
    snprintf(msg, sizeof(msg), "err %u", (unsigned)err);
    paintOtaScreen("OTA SELHALO", msg);
    delay(1200);
    leaveOtaUiMode(true);
    netMqttSetEnabled(true);
    netMqttConnect();
  });
}

}  // namespace

void netOtaInit(void) {
  // begin až po Wi‑Fi
}

bool netOtaIsBusy(void) { return s_busy; }

void netOtaTick(void) {
  if (!netWifiIsConnected()) {
    s_started = false;
    return;
  }

  if (!s_started) {
    setupCallbacks();
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    if (OTA_PASSWORD[0]) {
      ArduinoOTA.setPassword(OTA_PASSWORD);
    }
    ArduinoOTA.setMdnsEnabled(true);
    ArduinoOTA.begin();
    s_started = true;
    Serial.printf("[OTA] ArduinoOTA ready host=%s ip=%s (pio espota)\n",
                  OTA_HOSTNAME, netWifiIp());
  }

  ArduinoOTA.handle();
}
