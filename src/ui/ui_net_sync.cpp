// ui_net_sync.cpp — FreeRTOS net task (Wi-Fi/NTP/MQTT/room sensor)
#include "ui_net_sync.h"

#include "bus_lg_config.h"
#include "climate_plan.h"
#include "climate_regulator.h"
#include "climate_room.h"
#if LG_HAS_H2_UART_ROOM
#include "climate_room_uart.h"
#endif
#include "lg_board.h"
#include "net_mqtt_client.h"
#include "net_ntp_time.h"
#include "net_ota.h"
#include "net_wifi_mgr.h"
#include "src/ui_ui_lvgl.h"

#if LG_HAS_SDIO_ARBITER
#include "src/net_sdio_arbiter.h"
#endif

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace {

static const char* TAG = "NET";
TaskHandle_t s_netTask = nullptr;

bool copyIfChanged(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0 || !src) { return false; }
  if (strcmp(dst, src) == 0) { return false; }
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
  return true;
}

bool netWatchActive(void) {
#if LG_HAS_SDIO_ARBITER
  return netSdioMqttWanted();
#else
  return netMqttIsWatchActive();
#endif
}

bool netRadioBusy(void) {
#if LG_HAS_SDIO_ARBITER
  return netSdioTlsBusy() || netSdioRadioBusy() || netOtaIsBusy();
#else
  return netWifiIsBusy() || netMqttIsBusy() || climateRoomIsBusy() || netOtaIsBusy();
#endif
}

void netTask(void* /*arg*/) {
  vTaskDelay(pdMS_TO_TICKS(800));
  ESP_LOGI(TAG, "task start core=%d", (int)xPortGetCoreID());

  uint32_t freezeUntilMs = 0;
  uint32_t lastHbMs = 0;

  for (;;) {
    const bool wifiBusy = netWifiIsBusy();
    const bool mqttBusy = netMqttIsBusy();
    const bool roomBusy = climateRoomIsBusy();
    const uint32_t now = millis();

#if LG_USE_EEZ_LVGL
    if (roomBusy) {
#if !LG_HAS_SDIO_ARBITER
      uiLvglSetSdioLight(true);
#endif
      if (uiLvglIsFrozen()) {
        uiLvglSetFrozen(false);
      }
      freezeUntilMs = 0;
    } else if (wifiBusy) {
      if (!uiLvglIsFrozen() && freezeUntilMs == 0) {
        freezeUntilMs = now + 3500;
        uiLvglSetFrozen(true);
        ESP_LOGI(TAG, "LVGL freeze (Wi-Fi)");
      } else if (uiLvglIsFrozen() && now >= freezeUntilMs && !mqttBusy) {
        uiLvglSetFrozen(false);
        ESP_LOGI(TAG, "LVGL unfreeze");
      }
    } else if (!mqttBusy) {
      freezeUntilMs = 0;
      if (uiLvglIsFrozen()) {
        uiLvglSetFrozen(false);
      }
    }
#endif

    netWifiTick();
    netNtpTick();
    netOtaTick();
    climateRoomTick();
    netMqttTick();

    if (now - lastHbMs >= 3000) {
      lastHbMs = now;
      Serial.printf("[NET] HB wifi=%d mqtt=%d room=%d watch=%d heap=%u\r\n",
                    (int)netWifiIsConnected(),
                    (int)netMqttIsConnected(),
                    (int)climateRoomIsOk(),
                    (int)netWatchActive(),
                    (unsigned)ESP.getFreeHeap());
    }

    const bool busy = wifiBusy || mqttBusy || roomBusy || netRadioBusy();
    vTaskDelay(pdMS_TO_TICKS(busy ? 40 : 150));
  }
}

}  // namespace

void uiNetSyncWifi(void) {
  uiEez.set_wifi_enabled = netWifiIsEnabled();
  uiEez.sig_wifi = netWifiIsConnected();
  copyIfChanged(uiEez.set_wifi_status, sizeof(uiEez.set_wifi_status), netWifiStatus());
  copyIfChanged(uiEez.set_wifi_ssid, sizeof(uiEez.set_wifi_ssid), netWifiSsid());
  copyIfChanged(uiEez.set_wifi_ip, sizeof(uiEez.set_wifi_ip), netWifiIp());

  uiEez.set_mqtt_enabled = netMqttIsEnabled();
  uiEez.sig_mqtt = netMqttIsConnected();
  // Oko „někdo se dívá“ jen při živém MQTT + aktivním watch
  uiEez.sig_remote = uiEez.sig_mqtt && netWatchActive();
  copyIfChanged(uiEez.set_mqtt_status, sizeof(uiEez.set_mqtt_status), netMqttStatus());
  copyIfChanged(uiEez.set_mqtt_host, sizeof(uiEez.set_mqtt_host), netMqttHost());

  char hint[64];
  climateRoomStatusText(hint, sizeof(hint));
  copyIfChanged(uiEez.set_sys_hint, sizeof(uiEez.set_sys_hint), hint);
}

void uiNetInit(void) {
#if LG_HAS_SDIO_ARBITER
  netSdioInit();
#endif
  netWifiInit();
  netNtpInit();
  netMqttInit();
  climateRoomInit();
  climatePlanInit();
  climateRegulatorInit();
  uiNetSyncWifi();
}

void uiNetStartTask(void) {
  if (s_netTask) { return; }
  BaseType_t ok = xTaskCreatePinnedToCore(
      netTask, "net", LG_TASK_NET_STACK, nullptr,
      LG_TASK_NET_PRIO, &s_netTask, LG_CORE_NET);
  ESP_LOGI(TAG, "net task -> %d handle=%p", (int)ok, (void*)s_netTask);
}

void uiNetTick(void) {
  uiNetSyncWifi();
}

bool uiNetHandleAction(UiAkceTlacitko akce) {
  switch (akce) {
    case UI_AKCE_WIFI_TOGGLE:
      netWifiSetEnabled(!netWifiIsEnabled());
      if (!netWifiIsEnabled()) {
        netMqttSetEnabled(false);
      }
      uiNetSyncWifi();
      return true;
    case UI_AKCE_WIFI_CONNECT:
    case UI_AKCE_WIFI_FORM_CONNECT:
      if (!netWifiIsEnabled()) {
        netWifiSetEnabled(true);
      }
      netWifiConnect();
      uiNetSyncWifi();
      return true;
    case UI_AKCE_WIFI_FORM_SAVE:
      uiNetSyncWifi();
      return true;
    case UI_AKCE_MQTT_TOGGLE:
      netMqttSetEnabled(!netMqttIsEnabled());
      uiNetSyncWifi();
      return true;
    case UI_AKCE_MQTT_CONNECT:
      if (!netWifiIsConnected()) {
        strncpy(uiEez.set_mqtt_status, "Nejdriv Wi-Fi",
                sizeof(uiEez.set_mqtt_status) - 1);
        return true;
      }
      netMqttConnect();
      uiNetSyncWifi();
      return true;
    case UI_AKCE_SETTINGS_BLE:
#if LG_HAS_H2_UART_ROOM
      climateRoomStartScan();
#else
      climateRoomRequestNow();
      strncpy(uiEez.set_sys_hint, "Cekam senzor...",
              sizeof(uiEez.set_sys_hint) - 1);
      uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
#endif
      return true;
#if LG_HAS_H2_UART_ROOM
    case UI_AKCE_SETTINGS_METER1:
      climateRoomSelectMeter(1);
      return true;
    case UI_AKCE_SETTINGS_METER2:
      climateRoomSelectMeter(2);
      return true;
    case UI_AKCE_SETTINGS_METER3:
      climateRoomSelectMeter(3);
      return true;
#endif
    default:
      return false;
  }
}
