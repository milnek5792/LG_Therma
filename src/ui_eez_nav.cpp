#include "ui_eez_nav.h"

#include "net_sdio_arbiter.h"
#include "ui_eez_settings.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#include "ui_eez_energy.h"
#include "ui_eez_ui.h"
#include "ui_eez_wifi_form.h"
#include "ui_eez_ble_mac_form.h"
#include "ui_touch_tab5.h"
#include "ui_ui_lvgl.h"

enum ScreensEnum uiGetCurrentScreen() {
  return (enum ScreensEnum)(uiGetScreenIndex() + 1);
}

void uiNavigateTo(enum ScreensEnum screenId) {
  // Mimo MAIN odlož MQTT reconnect — na MAIN reconnect povolen.
  if (screenId == SCREEN_ID_MAIN) {
    netSdioEndUiHeavy();
  } else {
    netSdioBeginUiHeavy(20000);
  }
  netSdioClearUiFreeze();

  const bool leavingPlan =
      uiGetCurrentScreen() == SCREEN_ID_PLAN && screenId != SCREEN_ID_PLAN;
  const bool leavingReg =
      uiGetCurrentScreen() == SCREEN_ID_REGULATOR &&
      screenId != SCREEN_ID_REGULATOR;

  if (leavingPlan) {
    uiPlanOnLeave();
  }
  if (leavingReg) {
    uiRegulatorOnLeave();
  }

  if (screenId == SCREEN_ID_WIFI_SETUP) {
    uiWifiFormPrepare();
  }
  if (screenId == SCREEN_ID_BLE_MAC) {
    uiBleMacFormPrepare();
  }
  if (screenId == SCREEN_ID_PLAN) {
    uiPlanEnsureCreated();
  }
  if (screenId == SCREEN_ID_REGULATOR) {
    uiRegulatorEnsureCreated();
  }
  if (screenId == SCREEN_ID_SPOTREBA) {
    uiEnergyEnsureCreated();
  }

  loadScreen(screenId);

  if (leavingPlan) {
    uiPlanFlushSave();
  }
  if (leavingReg) {
    uiRegulatorFlushSave();
  }

  // Wi-Fi / MAC formulář = LVGL pointer (klávesnice). Ostatní = custom hit-test.
  const bool useLvglPointer =
      (screenId == SCREEN_ID_WIFI_SETUP || screenId == SCREEN_ID_BLE_MAC);
  uiLvglSetPointerInput(useLvglPointer);
  if (!useLvglPointer) {
    uiTouchVisualInit();
  }
}

bool uiIsMainScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_MAIN;
}

bool uiIsSettingsScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_SETTINGS;
}

bool uiIsWifiSetupScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_WIFI_SETUP;
}

bool uiIsPlanScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_PLAN;
}

bool uiIsRegulatorScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_REGULATOR;
}

bool uiIsEnergyScreen() {
  return uiGetCurrentScreen() == SCREEN_ID_SPOTREBA;
}
