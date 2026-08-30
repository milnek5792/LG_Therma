// net_sdio_arbiter.cpp — watch + TLS/UI policy (bez BLE / WiFi STOP)
#include "net_sdio_arbiter.h"

#include "mqtt_config.h"
#include "bus_lg_config.h"
#if LG_USE_EEZ_LVGL
#include "ui_ui_lvgl.h"
#endif

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace {

constexpr uint32_t kWatchDefaultMs = MQTT_WATCH_IDLE_MS;
constexpr uint32_t kWatchOffGraceMs = 15 * 1000;
constexpr uint32_t kUiHeavyDefaultMs = 2500;

portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
NetSdioOwner s_owner = NET_SDIO_NONE;

bool s_mqttSession = false;
bool s_tlsBusy = false;
bool s_watchForcedOff = false;
uint32_t s_watchUntilMs = 0;
uint32_t s_uiHeavyUntilMs = 0;
uint32_t s_uiFreezeUntilMs = 0;

bool s_uiLight = false;
bool s_uiFrozen = false;

bool watchActive() {
  if (s_watchForcedOff) { return false; }
  return (int32_t)(s_watchUntilMs - millis()) > 0;
}

bool uiHeavyActive() {
  return (int32_t)(s_uiHeavyUntilMs - millis()) > 0;
}

void setFrozen(bool on) {
#if LG_USE_EEZ_LVGL
  if (s_uiFrozen == on) { return; }
  s_uiFrozen = on;
  uiLvglSetFrozen(on);
#else
  s_uiFrozen = on;
#endif
}

void setUiLight(bool on) {
  if (s_uiLight == on) { return; }
  s_uiLight = on;
#if LG_USE_EEZ_LVGL
  uiLvglSetSdioLight(on);
#endif
}

void applyDisplayPolicy() {
  // Light jen při TLS / UI heavy — ne po celou MQTT session (zamrzlá grafika / Zpět).
  const bool busy = s_tlsBusy || uiHeavyActive();
  setUiLight(busy);
  const bool holdFreeze = (int32_t)(s_uiFreezeUntilMs - millis()) > 0;
  setFrozen(s_tlsBusy || holdFreeze);
}

}  // namespace

void netSdioInit(void) {
  s_owner = NET_SDIO_NONE;
  s_mqttSession = false;
  s_tlsBusy = false;
  s_watchForcedOff = false;
  s_watchUntilMs = 0;
  s_uiHeavyUntilMs = 0;
  s_uiFreezeUntilMs = 0;
  s_uiLight = false;
  s_uiFrozen = false;
  Serial.println("[SDIO] radic init (MQTT/UI — BLE externi H2)");
}

void netSdioHoldUiFreeze(uint32_t holdMs) {
  if (holdMs > 15000) { holdMs = 15000; }
  const uint32_t until = millis() + holdMs;
  if ((int32_t)(until - s_uiFreezeUntilMs) > 0) {
    s_uiFreezeUntilMs = until;
  }
  applyDisplayPolicy();
}

void netSdioClearUiFreeze(void) {
  s_uiFreezeUntilMs = 0;
  applyDisplayPolicy();
}

void netSdioBumpWatch(uint32_t holdMs) {
  if (holdMs < 1000) { holdMs = 1000; }
  s_watchForcedOff = false;
  const uint32_t until = millis() + holdMs;
  if ((int32_t)(until - s_watchUntilMs) > 0) {
    s_watchUntilMs = until;
  }
}

void netSdioBumpWatchDefault(void) {
  netSdioBumpWatch(kWatchDefaultMs);
}

bool netSdioMqttWanted(void) {
  return watchActive();
}

void netSdioWatchOff(void) {
  s_watchForcedOff = true;
  s_watchUntilMs = millis() + kWatchOffGraceMs;
}

void netSdioSetMqttSession(bool online) {
  portENTER_CRITICAL(&s_lock);
  s_mqttSession = online;
  if (online) {
    s_owner = NET_SDIO_MQTT;
  } else if (s_owner == NET_SDIO_MQTT && !s_tlsBusy) {
    s_owner = NET_SDIO_NONE;
  }
  portEXIT_CRITICAL(&s_lock);
  applyDisplayPolicy();
}

void netSdioSetTlsBusy(bool busy) {
  portENTER_CRITICAL(&s_lock);
  s_tlsBusy = busy;
  if (busy) {
    s_owner = NET_SDIO_MQTT;
  }
  portEXIT_CRITICAL(&s_lock);
  applyDisplayPolicy();
}

bool netSdioTlsBusy(void) { return s_tlsBusy; }
bool netSdioMqttSession(void) { return s_mqttSession; }

bool netSdioCanMqtt(void) {
  return !uiHeavyActive();
}

bool netSdioTryBeginMqtt(void) {
  if (!netSdioCanMqtt() && !s_mqttSession && !s_tlsBusy) {
    return false;
  }
  portENTER_CRITICAL(&s_lock);
  s_owner = NET_SDIO_MQTT;
  portEXIT_CRITICAL(&s_lock);
  applyDisplayPolicy();
  return true;
}

void netSdioEndMqtt(void) {
  portENTER_CRITICAL(&s_lock);
  if (!s_mqttSession && !s_tlsBusy) {
    s_owner = NET_SDIO_NONE;
  }
  portEXIT_CRITICAL(&s_lock);
  applyDisplayPolicy();
}

void netSdioBeginUiHeavy(uint32_t holdMs) {
  if (holdMs < 200) { holdMs = 200; }
  const uint32_t until = millis() + holdMs;
  if ((int32_t)(until - s_uiHeavyUntilMs) > 0) {
    s_uiHeavyUntilMs = until;
  }
  applyDisplayPolicy();
}

void netSdioBeginUiHeavyDefault(void) {
  netSdioBeginUiHeavy(kUiHeavyDefaultMs);
}

void netSdioEndUiHeavy(void) {
  s_uiHeavyUntilMs = 0;
  applyDisplayPolicy();
}

NetSdioOwner netSdioOwner(void) { return s_owner; }

size_t netSdioDmaMax(void) {
  return heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
}

NetSdioPressure netSdioPressure(void) {
  const size_t dma = netSdioDmaMax();
  if (dma < 12 * 1024) { return NET_SDIO_PRESSURE_CRITICAL; }
  if (dma < 24 * 1024) { return NET_SDIO_PRESSURE_LOW; }
  return NET_SDIO_PRESSURE_OK;
}

bool netSdioRadioBusy(void) {
  return s_tlsBusy || s_mqttSession || uiHeavyActive() ||
         (s_owner != NET_SDIO_NONE);
}

void netSdioTick(void) {
  if (!uiHeavyActive() && s_owner == NET_SDIO_UI) {
    portENTER_CRITICAL(&s_lock);
    if (s_owner == NET_SDIO_UI) { s_owner = NET_SDIO_NONE; }
    portEXIT_CRITICAL(&s_lock);
  }
  applyDisplayPolicy();
}
