#ifndef UI_TOUCH_TAB5_H
#define UI_TOUCH_TAB5_H

#include "lg_lvgl.h"
#include <stdint.h>

void uiTouchTab5Init();
void uiTouchTab5SetupCheck();
bool uiTouchTab5Poll();
uint32_t uiTouchTab5PollCount();
void uiTouchProcessEvents();
void uiTouchVisualInit();
void uiTouchVisualSync();

#endif
