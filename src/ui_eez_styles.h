#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Style: btn_run
lv_style_t *get_style_btn_run_MAIN_DEFAULT();
lv_style_t *get_style_btn_run_MAIN_PRESSED();
void add_style_btn_run(lv_obj_t *obj);
void remove_style_btn_run(lv_obj_t *obj);

// Style: btn_stop
lv_style_t *get_style_btn_stop_MAIN_DEFAULT();
lv_style_t *get_style_btn_stop_MAIN_PRESSED();
void add_style_btn_stop(lv_obj_t *obj);
void remove_style_btn_stop(lv_obj_t *obj);

// Style: btn_step
lv_style_t *get_style_btn_step_MAIN_DEFAULT();
void add_style_btn_step(lv_obj_t *obj);
void remove_style_btn_step(lv_obj_t *obj);

// Style: btn_menu
lv_style_t *get_style_btn_menu_MAIN_DEFAULT();
void add_style_btn_menu(lv_obj_t *obj);
void remove_style_btn_menu(lv_obj_t *obj);

// Style: btn_tichy
lv_style_t *get_style_btn_tichy_MAIN_DEFAULT();
void add_style_btn_tichy(lv_obj_t *obj);
void remove_style_btn_tichy(lv_obj_t *obj);

// Style: btn1
lv_style_t *get_style_btn1_MAIN_DEFAULT();
void add_style_btn1(lv_obj_t *obj);
void remove_style_btn1(lv_obj_t *obj);

// Style: text_24
lv_style_t *get_style_text_24_MAIN_DEFAULT();
void add_style_text_24(lv_obj_t *obj);
void remove_style_text_24(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/