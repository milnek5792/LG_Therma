// ui_display_mgr.h — jas + usínání displeje (backlight), wake na dotyk
#ifndef UI_DISPLAY_MGR_H
#define UI_DISPLAY_MGR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uiDisplayInit(void);
/** GT911 handle z uiLvglInit (opaque). */
void uiDisplayBindTouch(void* touch);
void uiDisplayTick(void);

void uiDisplayNoteActivity(void);
bool uiDisplayIsAsleep(void);
void uiDisplayWake(void);

uint8_t uiDisplayGetBrightness(void);
void uiDisplaySetBrightness(uint8_t percent, bool persist);

uint32_t uiDisplayGetSleepTimeoutSec(void);
void uiDisplaySetSleepTimeoutSec(uint32_t sec, bool persist);
/** Text pro UI: "Vypnuto" / "1 min" / ... */
const char* uiDisplaySleepTimeoutLabel(void);
/** Cyklus: Off → 1 → 2 → 5 → 10 → 30 min → Off */
void uiDisplayCycleSleepTimeout(void);

/** true = spánek: první stisk jen probudí (bez UI). */
bool uiDisplayHandleTouchWhileAsleep(bool pressed);

#ifdef __cplusplus
}
#endif

#endif
