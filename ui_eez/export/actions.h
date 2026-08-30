#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_akce_teplota_plus(lv_event_t * e);
extern void action_akce_teplota_minus(lv_event_t * e);
extern void action_akce_start_stop(lv_event_t * e);
extern void action_akce_tichy_rezim(lv_event_t * e);
extern void action_akce_menu(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/