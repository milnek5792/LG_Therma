#include "ui_eez_nav.h"

#include "net_sdio_arbiter.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#include "ui_eez_settings.h"
#include "ui_eez_ui.h"
#include "ui_eez_wifi_form.h"
#include "ui_panel_scale.h"
#include "ui_touch_tab5.h"
#include "ui_ui_lvgl.h"

enum ScreensEnum uiGetCurrentScreen() {
  return (enum ScreensEnum)(uiGetScreenIndex() + 1);
}

void uiNavigateTo(enum ScreensEnum screenId) {
  netSdioBeginUiHeavyDefault();
  const bool leavingPlan =
      uiGetCurrentScreen() == SCREEN_ID_PLAN && screenId != SCREEN_ID_PLAN;
  const bool leavingReg = uiGetCurrentScreen() == SCREEN_ID_REGULATOR &&
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
  if (screenId == SCREEN_ID_PLAN) {
    uiPlanEnsureCreated();
  }
  if (screenId == SCREEN_ID_REGULATOR) {
    uiRegulatorEnsureCreated();
  }
  loadScreen(screenId);
  uiLvglSetPointerInput(true);
  if (leavingPlan) {
    uiPlanFlushSave();
    uiLvglScheduleRecover("post_plan", false, 100);
    uiLvglScheduleRecover("post_plan_hard", true, 480);
  }
  if (screenId != SCREEN_ID_WIFI_SETUP) {
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
