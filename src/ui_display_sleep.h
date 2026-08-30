#ifndef UI_DISPLAY_SLEEP_H
#define UI_DISPLAY_SLEEP_H

#include "ui_display_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void uiDisplaySleepInit(void) { uiDisplayInit(); }
static inline void uiDisplaySleepTick(void) { uiDisplayTick(); }
static inline void uiDisplaySleepNotifyActivity(void) { uiDisplayNoteActivity(); }
static inline void uiDisplaySleepRefresh(void) {
  if (!uiDisplayIsAsleep()) {
    uiDisplayWake();
  }
}
static inline bool uiDisplaySleepIsAsleep(void) { return uiDisplayIsAsleep(); }

#ifdef __cplusplus
}
#endif

#endif
