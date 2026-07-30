// ui_panel_scale.h — škálování EEZ layoutu 1280×720 → 1024×600
#ifndef UI_PANEL_SCALE_H
#define UI_PANEL_SCALE_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Aplikuj transform scale na screen (pivot 0,0). */
void uiPanelScaleApply(lv_obj_t* screen);

/** Aplikuj na main / settings / wifi po ui_init(). */
void uiPanelScaleApplyAllScreens(void);

#ifdef __cplusplus
}
#endif

#endif
