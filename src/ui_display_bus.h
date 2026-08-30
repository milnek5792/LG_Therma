#ifndef UI_DISPLAY_BUS_H
#define UI_DISPLAY_BUS_H

#include "freertos/FreeRTOS.h"

// Mutex jen pro kresleni z UI task (core 0). Dotyk cte M5.Touch na core 1 bez mutexu.
void uiDisplayBusInit();
bool uiDisplayBusLock(TickType_t timeoutTicks);
void uiDisplayBusUnlock();

#endif
