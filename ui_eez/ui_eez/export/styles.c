#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: btn_run
//

void init_style_btn_run_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x30d158));
    lv_style_set_text_color(style, lv_color_hex(0x000000));
    lv_style_set_radius(style, 8);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(style, &lv_font_montserrat_18);
    lv_style_set_border_width(style, 0);
};

lv_style_t *get_style_btn_run_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_run_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_btn_run_MAIN_PRESSED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x28a745));
};

lv_style_t *get_style_btn_run_MAIN_PRESSED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_run_MAIN_PRESSED(style);
    }
    return style;
};

void add_style_btn_run(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn_run_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_btn_run_MAIN_PRESSED(), LV_PART_MAIN | LV_STATE_PRESSED);
};

void remove_style_btn_run(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn_run_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_btn_run_MAIN_PRESSED(), LV_PART_MAIN | LV_STATE_PRESSED);
};

//
// Style: btn_stop
//

void init_style_btn_stop_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x24242b));
    lv_style_set_text_color(style, lv_color_hex(0x636366));
    lv_style_set_radius(style, 8);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(style, &lv_font_montserrat_18);
    lv_style_set_border_width(style, 0);
};

lv_style_t *get_style_btn_stop_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_stop_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_btn_stop_MAIN_PRESSED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x6f7173));
};

lv_style_t *get_style_btn_stop_MAIN_PRESSED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_stop_MAIN_PRESSED(style);
    }
    return style;
};

void add_style_btn_stop(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn_stop_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_btn_stop_MAIN_PRESSED(), LV_PART_MAIN | LV_STATE_PRESSED);
};

void remove_style_btn_stop(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn_stop_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_btn_stop_MAIN_PRESSED(), LV_PART_MAIN | LV_STATE_PRESSED);
};

//
// Style: btn_step
//

void init_style_btn_step_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x24242b));
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_radius(style, 50);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(style, &lv_font_montserrat_40);
    lv_style_set_border_width(style, 2);
    lv_style_set_border_color(style, lv_color_hex(0x3a3a45));
};

lv_style_t *get_style_btn_step_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_step_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_btn_step(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn_step_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_btn_step(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn_step_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: btn_menu
//

void init_style_btn_menu_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x24242b));
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_radius(style, 0);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(style, &lv_font_montserrat_18);
    lv_style_set_border_width(style, 0);
};

lv_style_t *get_style_btn_menu_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_menu_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_btn_menu(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn_menu_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_btn_menu(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn_menu_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: btn_tichy
//

void init_style_btn_tichy_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0x0a84ff));
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_radius(style, 6);
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(style, &lv_font_montserrat_14);
    lv_style_set_border_width(style, 0);
};

lv_style_t *get_style_btn_tichy_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn_tichy_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_btn_tichy(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn_tichy_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_btn_tichy(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn_tichy_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: btn1
//

void init_style_btn1_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_radius(style, 40);
    lv_style_set_bg_color(style, lv_color_hex(0x1a1a1f));
    lv_style_set_border_color(style, lv_color_lighten(lv_color_hex(0x24242b), 64));
    lv_style_set_border_width(style, 2);
    lv_style_set_border_opa(style, 255);
};

lv_style_t *get_style_btn1_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_btn1_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_btn1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_btn1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_btn1(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_btn1_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: text_24
//

void init_style_text_24_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &lv_font_montserrat_14);
    lv_style_set_text_color(style, lv_color_hex(0x8e8e93));
    lv_style_set_align(style, LV_ALIGN_LEFT);
};

lv_style_t *get_style_text_24_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_text_24_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_text_24(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_text_24_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_text_24(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_text_24_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_btn_run,
        add_style_btn_stop,
        add_style_btn_step,
        add_style_btn_menu,
        add_style_btn_tichy,
        add_style_btn1,
        add_style_text_24,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_btn_run,
        remove_style_btn_stop,
        remove_style_btn_step,
        remove_style_btn_menu,
        remove_style_btn_tichy,
        remove_style_btn1,
        remove_style_text_24,
    };
    remove_style_funcs[styleIndex](obj);
}