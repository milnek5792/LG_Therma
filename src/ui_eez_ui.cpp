#include "ui_eez_ui.h"

#include "app_cmd.h"
#include "ui_eez_screens.h"
#include "ui_eez_settings.h"
#include "ui_eez_wifi_form.h"
#include "ui_eez_ble_mac_form.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#include "ui_eez_energy.h"
#include "ui_eez_images.h"
#include "ui_eez_actions.h"
#include "ui_eez_vars.h"

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
  if (index == 5) {
    return uiBleMacFormScreen();
  }
  if (index == 6) {
    return uiEnergyScreen();
  }
  return nullptr;
}

void loadScreen(enum ScreensEnum screenId) {
  currentScreen = screenId - 1;
  lv_obj_t* screen = getLvglObjectFromIndex(currentScreen);
  if (screen) {
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
      if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_indev_reset(indev, nullptr);
      }
      indev = lv_indev_get_next(indev);
    }
    lv_screen_load(screen);
  }
}

void ui_init() {
  create_screens();
  uiSettingsCreate();
  uiWifiFormCreate();
  uiBleMacFormCreate();
  loadScreen(SCREEN_ID_MAIN);
}

void ui_tick() {
  tick_screen(currentScreen);
  appCmdDrainUi();
}
