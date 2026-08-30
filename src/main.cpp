// main.cpp — LG Therma Tab5 (PlatformIO / pioarduino)
#include <Arduino.h>
#include "M5Unified.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/brownout.h"

#include "app_version.h"
#include "bus_lg_config.h"
#include "src/bus_lg_model.h"
#include "src/bus_lg_lin_api.h"
#include "bus_lg_task.h"
#include "ui_task_ui.h"
#include "app_cmd.h"
#include "ui_bus_bindings.h"
#include "ui_net_sync.h"

#include "src/net_mqtt_client.h"
#include "src/net_sdio_arbiter.h"
#include "src/ui_eez_model.h"
#include "src/ui_ui_lvgl.h"

#if LG_USE_EEZ_LVGL
#include "src/ui_touch_tab5.h"
#include "src/ui_display_sleep.h"
#else
#include "src/ui_touch_hmi.h"
#include "src/ui_hmi_draw.h"
#endif

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3000)) { delay(10); }

#if LG_HAS_M5UNIFIED
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Touch.setHoldThresh(800);
  M5.Touch.setFlickThresh(24);
#endif

  esp_brownout_disable();

#if LG_HAS_M5UNIFIED
  netMqttReserveTlsMemory();
#endif

#if !LG_USE_EEZ_LVGL
  uiHmiDrawInit();
#endif

  uiEezInit();
  appCmdInit();

  Serial.println();
  Serial.printf("=== LG THERMA Tab5 FW %s ===\n", APP_FW_VERSION);
  Serial.println(" LG THERMA — PlatformIO / pioarduino");
  Serial.println(" Board: M5Stack Tab5 + H2 UART");
  Serial.println("==================================================\n");

  lgModelInit();
  lgModelRestoreSessionFromNvs();

#if LG_USE_EEZ_LVGL
  uiTouchTab5Init();
  uiTouchTab5SetupCheck();
  lgTaskUiInitDisplej();
  lgTaskUiStart();
  for (int i = 0; i < 500 && !uiLvglInitDone(); ++i) {
    delay(10);
  }
  if (!uiLvglInitDone()) {
    Serial.println("[LVGL] init timeout!");
  }
#else
  uiTouchHmiSetupCheck();
  lgTaskUiInitDisplej();
#endif

  uiNetInit();
  uiNetStartTask();

#if LG_LIN_DEDICATED_TASK
  lgBusStartTask();
#else
  lgBusInit();
#endif

  Serial.println("[setup] done — LIN task + net task + loop(ctrl/touch)");
}

void loop() {
#if LG_HAS_M5UNIFIED
  // Touch vždy — i během MQTT TLS (lg_ui je suspendované, flush je zamrznutý).
  M5.update();
#endif

  if (lgBusIsReady()) {
    uiBusBindingsTick();
    uiBusFlushDeferredStorage();
  } else {
    appCmdDrainCtrl();
  }

#if LG_USE_EEZ_LVGL
  uiTouchTab5Poll();
  uiDisplayTick();
#endif

  uiSerialMonitorPoll();
  delay(3);
}
