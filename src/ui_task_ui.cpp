#include "ui_task_ui.h"

#include "net_ota.h"

#include "app_cmd.h"
#include "bus_lg_config.h"
#include "ui_bus_bindings.h"
#include "ui_net_sync.h"
#include "src/bus_lg_lin_api.h"
#include "src/bus_lg_model.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <Arduino.h>

#if LG_USE_EEZ_LVGL
#include "src/ui_ui_lvgl.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#endif

static bool predDrzetPanel = false;
static bool predCekamePanel = false;
static TaskHandle_t s_uiTask = nullptr;

void uiSerialMonitorPoll() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'p' || c == 'P') {
      nastavMonitorPozastaven(true);
#if !LG_USE_EEZ_LVGL
      potrebaObnovitDisplej = true;
#endif
    } else if (c == 'r' || c == 'R' || c == 'c' || c == 'C') {
      nastavMonitorPozastaven(false);
#if !LG_USE_EEZ_LVGL
      potrebaObnovitDisplej = true;
#endif
    } else if (c == ' ') {
      prepniMonitorPozastaven();
#if !LG_USE_EEZ_LVGL
      potrebaObnovitDisplej = true;
#endif
    } else if (c == 's' || c == 'S') {
      prepniSoloRezim();
#if !LG_USE_EEZ_LVGL
      potrebaObnovitDisplej = true;
#endif
    } else if (c == 'd' || c == 'D') {
      prepniDrzetStav();
#if !LG_USE_EEZ_LVGL
      potrebaObnovitDisplej = true;
#endif
    }
  }
}

void lgTaskUiInitDisplej() {
  /* uiEezInit + appCmdInit v setup() */
}

static void lgTaskUiInitDisplejOnCore() {
#if LG_USE_EEZ_LVGL
  uiLvglInit();
  Serial.printf("[LVGL] %dx%d flush=%lu\n",
                uiLvglHorRes(), uiLvglVerRes(),
                (unsigned long)uiLvglFlushCount());
  uiNetSyncWifi();
#else
  uiBusBindingsTick();
  Serial.println("[UI] bindings only");
#endif
  Serial.println("[UI] task ready");
}

static void lgTaskUi(void* param) {
  (void)param;
  lgTaskUiInitDisplejOnCore();

  for (;;) {
#if LG_USE_EEZ_LVGL
    uiLvglTick();
    uiNetSyncWifi();
    uiRegulatorTick();
#else
    if (drzetStavAktivni != predDrzetPanel || cekameNaOrigStart != predCekamePanel) {
      predDrzetPanel = drzetStavAktivni;
      predCekamePanel = cekameNaOrigStart;
      potrebaObnovitDisplej = true;
    }
    uiBusBindingsTick();
#endif
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}

void lgTaskUiStart() {
  xTaskCreatePinnedToCore(
      lgTaskUi, "lg_ui", LG_TASK_UI_STACK, nullptr,
      LG_TASK_UI_PRIO, &s_uiTask, LG_CORE_UI);
}

void lgTaskUiSuspend(void) {
  if (s_uiTask) {
    vTaskSuspend(s_uiTask);
    Serial.println("[UI] suspend (MQTT/TLS)");
  }
}

void lgTaskUiResume(void) {
  if (netOtaIsBusy()) {
    return;
  }
  if (s_uiTask) {
    vTaskResume(s_uiTask);
    Serial.println("[UI] resume");
  }
}
