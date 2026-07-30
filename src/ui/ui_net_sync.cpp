#include "ui_net_sync.h"

#include "climate_ble_room.h"
#include "net_mqtt_client.h"
#include "net_ntp_time.h"
#include "net_wifi_mgr.h"
#include "ui_ui_lvgl.h"

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace {

static const char* TAG = "NET";
TaskHandle_t s_netTask = nullptr;

bool copyIfChanged(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0 || !src) {
    return false;
  }
  if (strcmp(dst, src) == 0) {
    return false;
  }
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
  return true;
}

void netTask(void* /*arg*/) {
  vTaskDelay(pdMS_TO_TICKS(800));
  ESP_LOGI(TAG, "task start (core 0)");

  uint32_t freezeUntilMs = 0;
  uint32_t lastHbMs = 0;

  for (;;) {
    const bool wifiBusy = netWifiIsBusy();
    const bool mqttBusy = netMqttIsBusy();
    const bool bleBusy = climateBleIsBusy();
    const uint32_t now = millis();

    if (wifiBusy || bleBusy) {
      if (!uiLvglIsFrozen() && freezeUntilMs == 0) {
        freezeUntilMs = now + (bleBusy ? 8000 : 3500);
        uiLvglSetFrozen(true);
        ESP_LOGI(TAG, "LVGL freeze (%s)", bleBusy ? "BLE" : "Wi-Fi");
      } else if (uiLvglIsFrozen() && now >= freezeUntilMs && !mqttBusy &&
                 !bleBusy) {
        uiLvglSetFrozen(false);
        ESP_LOGI(TAG, "LVGL unfreeze");
      }
    } else if (!mqttBusy) {
      freezeUntilMs = 0;
      if (uiLvglIsFrozen()) {
        uiLvglSetFrozen(false);
      }
    }

    netWifiTick();
    netNtpTick();
    netMqttTick();
    climateBleTick();

    if (now - lastHbMs >= 3000) {
      lastHbMs = now;
      ESP_LOGI(TAG,
               "HB ms=%lu wifi=%d/%s mqtt=%d/%s ble=%d freeze=%d flush=%u heap=%u",
               (unsigned long)now,
               (int)netWifiIsConnected(), netWifiStatus(),
               (int)netMqttIsConnected(), netMqttStatus(),
               (int)climateBleIsOk(),
               (int)uiLvglIsFrozen(),
               (unsigned)uiLvglFlushCount(),
               (unsigned)ESP.getFreeHeap());
    }

    const bool busy = wifiBusy || mqttBusy || bleBusy || netMqttIsBusy();
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
  copyIfChanged(uiEez.set_mqtt_status, sizeof(uiEez.set_mqtt_status), netMqttStatus());
  copyIfChanged(uiEez.set_mqtt_host, sizeof(uiEez.set_mqtt_host), netMqttHost());

  char bleHint[64];
  climateBleStatusText(bleHint, sizeof(bleHint));
  copyIfChanged(uiEez.set_sys_hint, sizeof(uiEez.set_sys_hint), bleHint);
}

void uiNetInit(void) {
  netWifiInit();
  netNtpInit();
  netMqttInit();
  climateBleInit();
  uiNetSyncWifi();
}

void uiNetStartTask(void) {
  if (s_netTask) {
    return;
  }
  // Větší stack — TLS + NimBLE scan
  BaseType_t ok =
      xTaskCreatePinnedToCore(netTask, "net", 28672, nullptr, 1, &s_netTask, 0);
  ESP_LOGI(TAG, "xTaskCreatePinnedToCore -> %d handle=%p", (int)ok,
           (void*)s_netTask);
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
      climateBleRequestNow();
      strncpy(uiEez.set_sys_hint, "Skenuji SwitchBot...",
              sizeof(uiEez.set_sys_hint) - 1);
      uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
      return true;
    default:
      return false;
  }
}
