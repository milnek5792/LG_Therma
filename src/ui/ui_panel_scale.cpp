// ui_panel_scale.cpp — EEZ main zůstává 1280×720 (ořez); settings je nativně 1024×600
#include "ui_panel_scale.h"

#include <esp_log.h>

void uiPanelScaleApply(lv_obj_t* screen) {
  (void)screen;
}

void uiPanelScaleApplyAllScreens(void) {
  // Geometrické škálování vypnuto — hlavní UI upravíme v EEZ na 1024×600.
  ESP_LOGI("UI", "scale off — main=EEZ 1280x720 crop, settings=native 1024x600");
}
