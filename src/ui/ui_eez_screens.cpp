#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>

#include "ui_eez_screens.h"
#include "ui_eez_images.h"
#include "ui_eez_fonts.h"
#include "ui_eez_actions.h"
#include "ui_eez_vars.h"
#include "ui_eez_styles.h"
#include "ui_eez_ui.h"

#include <string.h>

objects_t objects;

lv_obj_t *tick_value_change_obj;

static lv_obj_t *makeTempUnitLabel(lv_obj_t *parent, uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_static(obj, "°C");
    return obj;
}

static void alignTempUnit(lv_obj_t *valueLbl, lv_obj_t *unitLbl) {
    if (!valueLbl || !unitLbl) {
        return;
    }
    lv_obj_align_to(unitLbl, valueLbl, LV_ALIGN_OUT_RIGHT_TOP, 4, 2);
}

static void setTempValueAndUnit(lv_obj_t *valueLbl, lv_obj_t *unitLbl, const char *new_val,
                               uint32_t onlineColor) {
    const char *cur_val = lv_label_get_text(valueLbl);
    if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = valueLbl;
        lv_label_set_text(valueLbl, new_val);
        tick_value_change_obj = NULL;
        alignTempUnit(valueLbl, unitLbl);
    }
    const uint32_t col =
        (strcmp(new_val, "___") == 0 || strcmp(new_val, "off") == 0 ||
         strcmp(new_val, "---") == 0)
            ? 0x8e8e93u
            : onlineColor;
    lv_obj_set_style_text_color(valueLbl, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (unitLbl) {
        lv_obj_set_style_text_color(unitLbl, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void create_screen_main() {
    // screen_main
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1024, 600);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x121214), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // panel_screen
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.panel_screen = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 1024, 600);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x121214), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // panel_top
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.panel_top = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 1024, 60);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_lighten(lv_color_hex(0x1a1a1f), 5), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x24242b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // panel_bottom
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.panel_bottom = obj;
            lv_obj_set_pos(obj, 0, 480);
            lv_obj_set_size(obj, 772, 120);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_lighten(lv_color_hex(0x1a1a1f), 5), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x24242b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // button divider_3
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.button_divider_3 = obj;
                    lv_obj_set_pos(obj, 566, 3);
                    lv_obj_set_size(obj, 2, 120);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_lighten(lv_color_hex(0x2f3237), 0), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // button divider_2
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.button_divider_2 = obj;
                    lv_obj_set_pos(obj, 175, 0);
                    lv_obj_set_size(obj, 2, 120);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_lighten(lv_color_hex(0x2f3237), 0), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // button divider_1
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.button_divider_1 = obj;
                    lv_obj_set_pos(obj, 372, 0);
                    lv_obj_set_size(obj, 2, 120);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_lighten(lv_color_hex(0x2f3237), 0), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // panel_tech
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.panel_tech = obj;
            lv_obj_set_pos(obj, 1329, -18);
            lv_obj_set_size(obj, 430, 540);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_lighten(lv_color_hex(0x1a1a1f), 5), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x2f3237), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // divider_main
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.divider_main = obj;
            lv_obj_set_pos(obj, 850, 60);
            lv_obj_set_size(obj, 2, 540);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x24242b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_cas
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_cas = obj;
            lv_obj_set_pos(obj, 30, 16);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_wifi
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_wifi = obj;
            lv_obj_set_pos(obj, 385, 15);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x8e8e93), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_mqtt
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_mqtt = obj;
            lv_obj_set_pos(obj, 572, 16);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x8e8e93), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_setpoint_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_setpoint_title = obj;
            lv_obj_set_pos(obj, -228, -165);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Nastavení teploty vody");
        }
        {
            // btn_minus
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_minus = obj;
            lv_obj_set_pos(obj, 64, 151);
            lv_obj_set_size(obj, 100, 100);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(obj, action_akce_teplota_minus, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            add_style_btn1(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj1 = obj;
                    lv_obj_set_pos(obj, 0, -6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
                    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "-");
                }
            }
        }
        {
            // lbl_setpoint
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_setpoint = obj;
            lv_obj_set_pos(obj, -245, -92);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_setpoint_unit
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_setpoint_unit = obj;
            lv_obj_set_pos(obj, 329, 200);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "°C");
        }
        {
            // lbl_water_sp — Auto: aktuální SP výstupní vody (z regulátoru / LIN)
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_water_sp = obj;
            lv_obj_set_pos(obj, -228, -40);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x8e8e93), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "");
        }
        {
            // btn_plus
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_plus = obj;
            lv_obj_set_pos(obj, 408, 152);
            lv_obj_set_size(obj, 100, 100);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(obj, action_akce_teplota_plus, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            add_style_btn1(obj);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1f), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj2 = obj;
                    lv_obj_set_pos(obj, -3, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
                    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "+");
                }
            }
        }
        {
            // panel_plan
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.panel_plan = obj;
            lv_obj_set_pos(obj, 16, 348);
            lv_obj_set_size(obj, 541, 90);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1f), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x24242b), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_plan_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_plan_title = obj;
            lv_obj_set_pos(obj, 36, 367);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            add_style_text_24(obj);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff9f0a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "TÝDENNÍ PLÁN AKTIVNÍ");
        }
        {
            // lbl_plan_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_plan_text = obj;
            lv_obj_set_pos(obj, 36, 394);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xaeaeae), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // btn_run
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_run = obj;
            lv_obj_set_pos(obj, 607, 90);
            lv_obj_set_size(obj, 195, 60);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(obj, action_akce_start, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            add_style_btn_run(obj);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1f), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_image_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x30d158), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj3 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
                    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "START");
                }
            }
        }
        {
            // btn_stop
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_stop = obj;
            lv_obj_set_pos(obj, 812, 90);
            lv_obj_set_size(obj, 195, 60);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(obj, action_akce_stop, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            add_style_btn_stop(obj);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1f), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0xff453a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj4 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
                    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "STOP");
                }
            }
        }
        {
            // led_chod
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led_chod = obj;
            lv_obj_set_pos(obj, 612, 185);
            lv_obj_set_size(obj, 14, 14);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW));
        }
        {
            // lbl_chod
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_chod = obj;
            lv_obj_set_pos(obj, 643, 179);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xe0e0e6), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "ZAPNUTO");
        }
        {
            // led_cerpadlo
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led_cerpadlo = obj;
            lv_obj_set_pos(obj, 612, 227);
            lv_obj_set_size(obj, 14, 14);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW));
        }
        {
            // lbl_cerpadlo
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_cerpadlo = obj;
            lv_obj_set_pos(obj, 643, 221);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xe0e0e6), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "OBĚHOVÉ ČERPADLO");
        }
        {
            // led_kompresor
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led_kompresor = obj;
            lv_obj_set_pos(obj, 612, 269);
            lv_obj_set_size(obj, 14, 14);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW));
        }
        {
            // lbl_kompresor
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_kompresor = obj;
            lv_obj_set_pos(obj, 646, 264);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xe0e0e6), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "KOMPRESOR");
        }
        {
            // led_odmrazovani
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led_odmrazovani = obj;
            lv_obj_set_pos(obj, 612, 311);
            lv_obj_set_size(obj, 14, 14);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW));
        }
        {
            // lbl_odmrazovani
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_odmrazovani = obj;
            lv_obj_set_pos(obj, 648, 306);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xe0e0e6), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "ODMRAZOVÁNÍ");
        }
        {
            // led_el_topeni
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led_el_topeni = obj;
            lv_obj_set_pos(obj, 612, 353);
            lv_obj_set_size(obj, 14, 14);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW));
        }
        {
            // lbl_el_topeni
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_el_topeni = obj;
            lv_obj_set_pos(obj, 650, 348);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xe0e0e6), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "PŘÍDAVNÉ EL. TOPENÍ");
        }
        {
            // lbl_vnitrni_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vnitrni_title = obj;
            lv_obj_set_pos(obj, 23, 501);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            add_style_text_24(obj);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Vnitřní teplota");
        }
        {
            // lbl_vnitrni_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vnitrni_value = obj;
            lv_obj_set_pos(obj, 49, 526);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_vnitrni_unit
            lv_obj_t *obj = makeTempUnitLabel(parent_obj, 0xffffffu);
            objects.lbl_vnitrni_unit = obj;
            alignTempUnit(objects.lbl_vnitrni_value, obj);
        }
        {
            // lbl_venkovni_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_venkovni_title = obj;
            lv_obj_set_pos(obj, 219, 501);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            add_style_text_24(obj);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Venkovní teplota");
        }
        {
            // lbl_venkovni_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_venkovni_value = obj;
            lv_obj_set_pos(obj, 251, 526);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x64d2ff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_vstup_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vstup_title = obj;
            lv_obj_set_pos(obj, 414, 501);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            add_style_text_24(obj);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x8e8e93), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Teplota vstup");
        }
        {
            // lbl_vstup_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vstup_value = obj;
            lv_obj_set_pos(obj, 419, 526);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_vstup_unit
            lv_obj_t *obj = makeTempUnitLabel(parent_obj, 0xffffffu);
            objects.lbl_vstup_unit = obj;
            alignTempUnit(objects.lbl_vstup_value, obj);
        }
        {
            // lbl_vystup_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vystup_title = obj;
            lv_obj_set_pos(obj, 606, 501);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            add_style_text_24(obj);
            lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Teplota výstup");
        }
        {
            // lbl_vystup_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_vystup_value = obj;
            lv_obj_set_pos(obj, 617, 526);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff9f0a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_vystup_unit
            lv_obj_t *obj = makeTempUnitLabel(parent_obj, 0xff9f0au);
            objects.lbl_vystup_unit = obj;
            alignTempUnit(objects.lbl_vystup_value, obj);
        }
        {
            // btn_menu — pravý dolní roh, výška = spodní pruh (120 px)
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_menu = obj;
            lv_obj_set_pos(obj, 850, 480);
            lv_obj_set_size(obj, 174, 120);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_add_event_cb(obj, action_akce_menu, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
            lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            add_style_btn_menu(obj);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1f), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0x24242b), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj5 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
                    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "MENU");
                }
            }
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
    {
        const char *new_val = get_var_cas_text();
        const char *cur_val = lv_label_get_text(objects.lbl_cas);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_cas;
            lv_label_set_text(objects.lbl_cas, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_sig_wifi____wi_fi__ok_____wi_fi______();
        const char *cur_val = lv_label_get_text(objects.lbl_wifi);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_wifi;
            lv_label_set_text(objects.lbl_wifi, new_val);
            tick_value_change_obj = NULL;
        }
        uint32_t col = get_var_sig_wifi_color();
        static uint32_t s_wifiCol = 0;
        if (s_wifiCol != col) {
            s_wifiCol = col;
            lv_obj_set_style_text_color(
                objects.lbl_wifi, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    {
        const char *new_val = get_var_sig_mqtt____mqtt__pripojeno_____mqtt______();
        const char *cur_val = lv_label_get_text(objects.lbl_mqtt);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_mqtt;
            lv_label_set_text(objects.lbl_mqtt, new_val);
            tick_value_change_obj = NULL;
        }
        uint32_t col = get_var_sig_mqtt_color();
        static uint32_t s_mqttCol = 0;
        if (s_mqttCol != col) {
            s_mqttCol = col;
            lv_obj_set_style_text_color(
                objects.lbl_mqtt, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    {
        const char *new_val = get_var_teplota_vody_set();
        const char *cur_val = lv_label_get_text(objects.lbl_setpoint);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_setpoint;
            lv_label_set_text(objects.lbl_setpoint, new_val);
            tick_value_change_obj = NULL;
            alignTempUnit(objects.lbl_setpoint, objects.lbl_setpoint_unit);
        }
        uint32_t col = get_var_teplota_vody_set_color();
        static uint32_t s_spCol = 0;
        if (s_spCol != col) {
            s_spCol = col;
            lv_obj_set_style_text_color(
                objects.lbl_setpoint, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
            if (objects.lbl_setpoint_unit) {
                lv_obj_set_style_text_color(
                    objects.lbl_setpoint_unit, lv_color_hex(col),
                    LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
    {
        const char *title = (get_var_rezim() == 0)
                                ? "Nastavení pokojové teploty"
                                : "Nastavení teploty vody";
        const char *cur = lv_label_get_text(objects.lbl_setpoint_title);
        if (!cur || strcmp(cur, title) != 0) {
            lv_label_set_text(objects.lbl_setpoint_title, title);
        }
    }
    {
        // Auto: malý řádek = SP vody (pending / potvrzené A0)
        if (objects.lbl_water_sp) {
            if (get_var_rezim() == 0) {
                lv_obj_remove_flag(objects.lbl_water_sp, LV_OBJ_FLAG_HIDDEN);
                char line[48];
                snprintf(line, sizeof(line), "Voda SP %s °C",
                         get_var_teplota_vody_set_lin());
                const char *cur = lv_label_get_text(objects.lbl_water_sp);
                if (!cur || strcmp(cur, line) != 0) {
                    lv_label_set_text(objects.lbl_water_sp, line);
                }
                uint32_t col = get_var_teplota_vody_set_lin_color();
                static uint32_t s_waterSpCol = 0;
                if (s_waterSpCol != col) {
                    s_waterSpCol = col;
                    lv_obj_set_style_text_color(
                        objects.lbl_water_sp, lv_color_hex(col),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            } else {
                lv_obj_add_flag(objects.lbl_water_sp, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    {
        const char *new_val = get_var_plan_text();
        const char *cur_val = lv_label_get_text(objects.lbl_plan_text);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_plan_text;
            lv_label_set_text(objects.lbl_plan_text, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_plan_title();
        const char *cur_val = lv_label_get_text(objects.lbl_plan_title);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_plan_title;
            lv_label_set_text(objects.lbl_plan_title, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = get_var_sig_chod___3199320___0x2c2c2e();
        if (hex != last_hex) {
            last_hex = hex;
            tick_value_change_obj = objects.led_chod;
            lv_led_set_color(objects.led_chod, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sig_chod___255___50();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.led_chod);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.led_chod;
            lv_led_set_brightness(objects.led_chod, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = get_var_sig_cerpadlo___3199320___0x2c2c2e();
        if (hex != last_hex) {
            last_hex = hex;
            tick_value_change_obj = objects.led_cerpadlo;
            lv_led_set_color(objects.led_cerpadlo, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sig_cerpadlo___255___50();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.led_cerpadlo);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.led_cerpadlo;
            lv_led_set_brightness(objects.led_cerpadlo, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = get_var_sig_kompresor___16752394___0x2c2c2e();
        if (hex != last_hex) {
            last_hex = hex;
            tick_value_change_obj = objects.led_kompresor;
            lv_led_set_color(objects.led_kompresor, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sig_kompresor___255___50();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.led_kompresor);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.led_kompresor;
            lv_led_set_brightness(objects.led_kompresor, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = get_var_sig_odmrazovani___3199320___0x2c2c2e();
        if (hex != last_hex) {
            last_hex = hex;
            tick_value_change_obj = objects.led_odmrazovani;
            lv_led_set_color(objects.led_odmrazovani, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sig_odmrazovani___255___50();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.led_odmrazovani);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.led_odmrazovani;
            lv_led_set_brightness(objects.led_odmrazovani, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = get_var_sig_el_topeni___16752394___0x2c2c2e();
        if (hex != last_hex) {
            last_hex = hex;
            tick_value_change_obj = objects.led_el_topeni;
            lv_led_set_color(objects.led_el_topeni, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sig_el_topeni___255___50();
        if (new_val < 0) new_val = 0;
        else if (new_val > 255) new_val = 255;
        int32_t cur_val = lv_led_get_brightness(objects.led_el_topeni);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.led_el_topeni;
            lv_led_set_brightness(objects.led_el_topeni, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        setTempValueAndUnit(objects.lbl_vnitrni_value, objects.lbl_vnitrni_unit,
                            get_var_teplota_vnitrni(), 0xffffffu);
    }
    {
        const char *new_val = get_var_teplota_venkovni();
        const char *cur_val = lv_label_get_text(objects.lbl_venkovni_value);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_venkovni_value;
            lv_label_set_text(objects.lbl_venkovni_value, new_val);
            tick_value_change_obj = NULL;
        }
        const uint32_t col =
            (strcmp(new_val, "___") == 0 || strcmp(new_val, "off") == 0 ||
             strcmp(new_val, "---") == 0)
                ? 0x8e8e93u
                : 0x64d2ffu;
        static uint32_t last_col = 0xFFFFFFFFu;
        if (col != last_col) {
            last_col = col;
            lv_obj_set_style_text_color(objects.lbl_venkovni_value, lv_color_hex(col),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    {
        setTempValueAndUnit(objects.lbl_vstup_value, objects.lbl_vstup_unit,
                            get_var_teplota_vody_vstup(), 0xffffffu);
    }
    {
        setTempValueAndUnit(objects.lbl_vystup_value, objects.lbl_vystup_unit,
                            get_var_teplota_vody_vystup(), 0xff9f0au);
    }
}

#include "ui_eez_settings.h"
#include "ui_eez_wifi_form.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    uiSettingsTick,
    uiWifiFormTick,
    uiPlanTick,
    uiRegulatorTick,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 5) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
    { "font_cs_16", &ui_font_font_cs_16 },
    { "font_cs_24", &ui_font_font_cs_24 },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_48 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_main();
}
#ifdef __cplusplus
}
#endif
