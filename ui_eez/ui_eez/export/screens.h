#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *panel_screen;
    lv_obj_t *panel_top;
    lv_obj_t *panel_bottom;
    lv_obj_t *button_divider_3;
    lv_obj_t *button_divider_2;
    lv_obj_t *button_divider_1;
    lv_obj_t *panel_tech;
    lv_obj_t *divider_main;
    lv_obj_t *lbl_cas;
    lv_obj_t *lbl_wifi;
    lv_obj_t *lbl_mqtt;
    lv_obj_t *btn_tichy;
    lv_obj_t *obj0;
    lv_obj_t *lbl_setpoint_title;
    lv_obj_t *btn_minus;
    lv_obj_t *obj1;
    lv_obj_t *lbl_setpoint;
    lv_obj_t *lbl_setpoint_unit;
    lv_obj_t *btn_plus;
    lv_obj_t *obj2;
    lv_obj_t *panel_plan;
    lv_obj_t *lbl_plan_title;
    lv_obj_t *lbl_plan_text;
    lv_obj_t *btn_run;
    lv_obj_t *obj3;
    lv_obj_t *btn_stop;
    lv_obj_t *obj4;
    lv_obj_t *led_chod;
    lv_obj_t *lbl_chod;
    lv_obj_t *led_cerpadlo;
    lv_obj_t *lbl_cerpadlo;
    lv_obj_t *led_kompresor;
    lv_obj_t *lbl_kompresor;
    lv_obj_t *led_odmrazovani;
    lv_obj_t *lbl_odmrazovani;
    lv_obj_t *led_el_topeni;
    lv_obj_t *lbl_el_topeni;
    lv_obj_t *lbl_vnitrni_title;
    lv_obj_t *lbl_vnitrni_value;
    lv_obj_t *lbl_venkovni_title;
    lv_obj_t *lbl_venkovni_value;
    lv_obj_t *lbl_vstup_title;
    lv_obj_t *lbl_vstup_value;
    lv_obj_t *lbl_vystup_title;
    lv_obj_t *lbl_vystup_value;
    lv_obj_t *btn_menu;
    lv_obj_t *obj5;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/