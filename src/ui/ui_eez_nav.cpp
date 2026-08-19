#include "ui_eez_nav.h"

#include "net_sdio_arbiter.h"
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
  if (screenId == SCREEN_ID_WIFI_SETUP) {
    uiWifiFormPrepare();
  }
  loadScreen(screenId);
  uiLvglSetPointerInput(true);
  // Bez full invalidate — po startu / přepnutí to zahltí refresh a touch „zamrzne“
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
