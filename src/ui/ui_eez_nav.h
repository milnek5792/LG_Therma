#ifndef UI_EEZ_NAV_H
#define UI_EEZ_NAV_H

#include "ui_eez_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ScreensEnum uiGetCurrentScreen();
void uiNavigateTo(enum ScreensEnum screenId);
bool uiIsMainScreen();
bool uiIsSettingsScreen();
bool uiIsWifiSetupScreen();
bool uiIsPlanScreen();

#ifdef __cplusplus
}
#endif

#endif
