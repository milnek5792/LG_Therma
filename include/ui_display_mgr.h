// ui_display_mgr.h — jas + usínání displeje (Tab5 M5Unified)
#ifndef UI_DISPLAY_MGR_H
#define UI_DISPLAY_MGR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uiDisplayInit(void);
void uiDisplayTick(void);

void uiDisplayNoteActivity(void);
bool uiDisplayIsAsleep(void);
void uiDisplayWake(void);

uint8_t uiDisplayGetBrightness(void);
void uiDisplaySetBrightness(uint8_t percent, bool persist);

uint32_t uiDisplayGetSleepTimeoutSec(void);
void uiDisplaySetSleepTimeoutSec(uint32_t sec, bool persist);
const char* uiDisplaySleepTimeoutLabel(void);
void uiDisplayCycleSleepTimeout(void);

#ifdef __cplusplus
}
#endif

#endif
