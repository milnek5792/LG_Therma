#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void action_akce_start_stop(lv_event_t * e);
extern void action_akce_start(lv_event_t * e);
extern void action_akce_stop(lv_event_t * e);
extern void action_akce_teplota_plus(lv_event_t * e);
extern void action_akce_tichy_rezim(lv_event_t * e);
extern void action_akce_menu(lv_event_t * e);
extern void action_akce_zpet(lv_event_t * e);
extern void action_akce_wifi_toggle(lv_event_t * e);
extern void action_akce_wifi_connect(lv_event_t * e);
extern void action_akce_wifi_edit(lv_event_t * e);
extern void action_akce_mqtt_toggle(lv_event_t * e);
extern void action_akce_mqtt_connect(lv_event_t * e);
extern void action_akce_settings_ble(lv_event_t * e);
extern void action_akce_settings_meter1(lv_event_t * e);
extern void action_akce_settings_meter2(lv_event_t * e);
extern void action_akce_settings_meter3(lv_event_t * e);
extern void action_akce_settings_ble_mac(lv_event_t * e);
extern void action_akce_settings_bridge_ota(lv_event_t * e);
extern void action_akce_settings_bridge_diag(lv_event_t * e);
extern void action_akce_settings_plan(lv_event_t * e);
extern void action_akce_teplota_minus(lv_event_t * e);
extern void action_akce_settings_servis(lv_event_t * e);
extern void action_akce_settings_spotreba(lv_event_t * e);
extern void action_akce_plan_back(lv_event_t * e);
extern void action_akce_plan_toggle(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/