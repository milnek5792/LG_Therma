#include "ui_eez_ui.h"

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
#include "ui_net_sync.h"
#include "ui_bus_bindings.h"
#include "climate_plan.h"

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

  const UiAkceTlacitko akce = uiEez.akce_tlacitko;
  if (akce == UI_AKCE_ZADNA) {
    return;
  }

  // Debounce: akci neshazuj — při odmítnutí nech v frontě na další tick
  static uint32_t s_lastAkceMs = 0;
  const uint32_t now = millis();
  if (s_lastAkceMs != 0 && (now - s_lastAkceMs) < 220) {
    return;
  }

  uiEez.akce_tlacitko = UI_AKCE_ZADNA;
  s_lastAkceMs = now;

  switch (akce) {
    case UI_AKCE_MENU:
      uiNavigateTo(SCREEN_ID_SETTINGS);
      break;
    case UI_AKCE_ZPET:
      uiNavigateTo(SCREEN_ID_MAIN);
      break;
    case UI_AKCE_WIFI_EDIT:
      uiNavigateTo(SCREEN_ID_WIFI_SETUP);
      break;
    case UI_AKCE_WIFI_FORM_BACK:
      uiNavigateTo(SCREEN_ID_SETTINGS);
      break;
    case UI_AKCE_WIFI_FORM_SAVE:
      uiNetHandleAction(akce);
      uiNavigateTo(SCREEN_ID_SETTINGS);
      break;
    case UI_AKCE_WIFI_FORM_CONNECT:
      // Nejdřív navigace (dokreslení), Wi‑Fi begin až po settle v ticku
      uiNavigateTo(SCREEN_ID_SETTINGS);
      uiNetHandleAction(akce);
      break;
    case UI_AKCE_WIFI_TOGGLE:
    case UI_AKCE_WIFI_CONNECT:
      uiNetHandleAction(akce);
      break;
    case UI_AKCE_MQTT_TOGGLE:
    case UI_AKCE_MQTT_CONNECT:
      uiNetHandleAction(akce);
      break;
    case UI_AKCE_SETTINGS_BLE:
      uiNetHandleAction(akce);
      break;
    case UI_AKCE_SETTINGS_PLAN:
      uiNavigateTo(SCREEN_ID_PLAN);
      break;
    case UI_AKCE_SETTINGS_SERVIS:
      uiNavigateTo(SCREEN_ID_REGULATOR);
      break;
    case UI_AKCE_PLAN_BACK:
      uiNavigateTo(SCREEN_ID_SETTINGS);
      break;
    case UI_AKCE_PLAN_TOGGLE:
      g_planConfig.aktivni = !g_planConfig.aktivni;
      uiPlanMarkDirty();
      uiPlanRefreshAll();
      break;
    case UI_AKCE_START_STOP:
    case UI_AKCE_START:
    case UI_AKCE_STOP:
    case UI_AKCE_TEPLOTA_PLUS:
    case UI_AKCE_TEPLOTA_MINUS:
    case UI_AKCE_REZIM_PREPNOUT:
      uiBusHandleAkce(akce);
      break;
    default:
      break;
  }
}
