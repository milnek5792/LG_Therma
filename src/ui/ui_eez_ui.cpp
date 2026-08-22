#include "ui_eez_ui.h"

#include "app_cmd.h"
#include "ui_eez_screens.h"
#include "ui_eez_settings.h"
#include "ui_eez_wifi_form.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#include "ui_eez_images.h"
#include "ui_eez_actions.h"
#include "ui_eez_vars.h"
#include "ui_eez_model.h"
#include "ui_eez_nav.h"

#include <Arduino.h>
#include <string.h>

static int16_t currentScreen = -1;

int uiGetScreenIndex() {
  return currentScreen;
}

static lv_obj_t* getLvglObjectFromIndex(int32_t index) {
  if (index == 0) {
    return objects.main;
  }
  if (index == 1) {
    return uiSettingsScreen();
  }
  if (index == 2) {
    return uiWifiFormScreen();
  }
  if (index == 3) {
    return uiPlanScreen();
  }
  if (index == 4) {
    return uiRegulatorScreen();
  }
  return nullptr;
}

void loadScreen(enum ScreensEnum screenId) {
  currentScreen = screenId - 1;
  lv_obj_t* screen = getLvglObjectFromIndex(currentScreen);
  if (screen) {
    // Po navigaci zruš aktivní touch — jinak se na novém screenu
    // omylem spustí tlačítko pod prstem
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
      if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_indev_reset(indev, nullptr);
      }
      indev = lv_indev_get_next(indev);
    }
    lv_screen_load(screen);
    // Jen dirty regiony obrazovky, ne full-paint storm
  }
}

void ui_init() {
  create_screens();
  uiSettingsCreate();
  uiWifiFormCreate();
#if LG_THERMA_BOOT_SETTINGS
  loadScreen(SCREEN_ID_SETTINGS);
#else
  loadScreen(SCREEN_ID_MAIN);
#endif
}

void ui_tick() {
  tick_screen(currentScreen);
  appCmdDrainUi();
}
