#include "ui_eez_ble_mac_form.h"

#include "app_cmd.h"
#include "climate_room_uart.h"
#include "h2_uart_protocol.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"
#include "ui_eez_nav.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

ble_mac_form_objects_t bleMacFormObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColOrange = 0xFF9F0Au;

const lv_font_t* kFont = &ui_font_font_cs_24;

void styleField(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(obj, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void styleActionBtn(lv_obj_t* obj, uint32_t bg) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(obj, 0);
  if (lbl) {
    lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, const char* text, uint32_t color) {
  lv_obj_t* obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_font(obj, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(obj, text);
  return obj;
}

lv_obj_t* makeActionButton(lv_obj_t* parent, int x, int y, int w, int h,
                           const char* text, lv_event_cb_t cb, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
  styleActionBtn(obj, bg);
  lv_obj_t* lbl = lv_label_create(obj);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  return obj;
}

void normalizeMac(char* mac, size_t len) {
  if (!mac || len < H2_MAC_STR_LEN) {
    return;
  }
  char tmp[32];
  size_t n = 0;
  for (size_t i = 0; mac[i] && n + 1 < sizeof(tmp); ++i) {
    const char c = mac[i];
    if (isxdigit((unsigned char)c)) {
      tmp[n++] = (char)toupper((unsigned char)c);
    }
  }
  tmp[n] = '\0';
  if (n != 12) {
    return;
  }
  snprintf(mac, len, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c", tmp[0], tmp[1], tmp[2],
           tmp[3], tmp[4], tmp[5], tmp[6], tmp[7], tmp[8], tmp[9], tmp[10],
           tmp[11]);
}

bool validMac(const char* mac) {
  if (!mac || strlen(mac) != 17) {
    return false;
  }
  for (int i = 0; i < 17; ++i) {
    if ((i % 3) == 2) {
      if (mac[i] != ':') {
        return false;
      }
    } else if (!isxdigit((unsigned char)mac[i])) {
      return false;
    }
  }
  return true;
}

void setHint(const char* text, uint32_t color) {
  if (!bleMacFormObj.lbl_hint) {
    return;
  }
  lv_label_set_text(bleMacFormObj.lbl_hint, text);
  lv_obj_set_style_text_color(bleMacFormObj.lbl_hint, lv_color_hex(color),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

void onBack(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_BLE_MAC_BACK);
}

void onSave(lv_event_t* e) {
  (void)e;
  if (!uiBleMacFormSave()) {
    return;
  }
  appCmdEnqueueHmi(UI_AKCE_BLE_MAC_SAVE);
}

void onFieldFocus(lv_event_t* e) {
  lv_obj_t* kb = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
  lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!kb || !ta) {
    return;
  }
  lv_keyboard_set_textarea(kb, ta);
}

}  // namespace

bool uiBleMacFormSave(void) {
  if (!bleMacFormObj.ta_room || !bleMacFormObj.ta_out) {
    return false;
  }
  char room[H2_MAC_STR_LEN];
  char out[H2_MAC_STR_LEN];
  const char* roomText = lv_textarea_get_text(bleMacFormObj.ta_room);
  const char* outText = lv_textarea_get_text(bleMacFormObj.ta_out);
  strncpy(room, roomText ? roomText : "", sizeof(room) - 1);
  room[sizeof(room) - 1] = '\0';
  strncpy(out, outText ? outText : "", sizeof(out) - 1);
  out[sizeof(out) - 1] = '\0';
  normalizeMac(room, sizeof(room));
  normalizeMac(out, sizeof(out));

  if (!validMac(room) || !validMac(out)) {
    setHint("MAC musí mít tvar AA:BB:CC:DD:EE:FF", kColOrange);
    return false;
  }

  lv_textarea_set_text(bleMacFormObj.ta_room, room);
  lv_textarea_set_text(bleMacFormObj.ta_out, out);

  const bool okRoom = climateRoomSetRoomMac(room);
  const bool okOut = climateRoomSetOutdoorMac(out);
  if (!okRoom || !okOut) {
    setHint("Uložení MAC selhalo", kColOrange);
    return false;
  }
  setHint("MAC uloženo - posílám na H2", kColGreen);
  Serial.printf("[ROOM] MAC form room=%s out=%s\n", room, out);
  return true;
}

void uiBleMacFormCreate(void) {
  memset(&bleMacFormObj, 0, sizeof(bleMacFormObj));
  lv_obj_t* scr = lv_obj_create(nullptr);
  bleMacFormObj.screen = scr;
  lv_obj_set_size(scr, 1280, 720);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  bleMacFormObj.btn_back = makeActionButton(
      scr, 40, 16, 220, 52, "<- ZPĚT", onBack, 0x48484Fu);

  bleMacFormObj.lbl_title = lv_label_create(scr);
  lv_label_set_text(bleMacFormObj.lbl_title, "SwitchBot MAC");
  lv_obj_set_style_text_font(bleMacFormObj.lbl_title, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(bleMacFormObj.lbl_title, lv_color_hex(kColText),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_align(bleMacFormObj.lbl_title, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_y(bleMacFormObj.lbl_title, 24);

  bleMacFormObj.lbl_room = makeLabel(scr, 40, 88, "Pokojový teploměr (MAC)", kColMuted);
  bleMacFormObj.ta_room = lv_textarea_create(scr);
  lv_obj_set_pos(bleMacFormObj.ta_room, 40, 120);
  lv_obj_set_size(bleMacFormObj.ta_room, 1200, 52);
  lv_textarea_set_one_line(bleMacFormObj.ta_room, true);
  lv_textarea_set_max_length(bleMacFormObj.ta_room, 17);
  styleField(bleMacFormObj.ta_room);

  bleMacFormObj.lbl_out = makeLabel(scr, 40, 184, "Venkovní teploměr (MAC)", kColMuted);
  bleMacFormObj.ta_out = lv_textarea_create(scr);
  lv_obj_set_pos(bleMacFormObj.ta_out, 40, 216);
  lv_obj_set_size(bleMacFormObj.ta_out, 1200, 52);
  lv_textarea_set_one_line(bleMacFormObj.ta_out, true);
  lv_textarea_set_max_length(bleMacFormObj.ta_out, 17);
  styleField(bleMacFormObj.ta_out);

  bleMacFormObj.btn_save = makeActionButton(
      scr, 40, 288, 300, 52, "Uložit", onSave, kColGreen);
  bleMacFormObj.lbl_hint = makeLabel(scr, 360, 300, "Formát AA:BB:CC:DD:EE:FF", kColMuted);

  bleMacFormObj.keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(bleMacFormObj.keyboard, 1280, 360);
  lv_obj_align(bleMacFormObj.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(bleMacFormObj.keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bleMacFormObj.keyboard, lv_color_hex(kColPanel),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(bleMacFormObj.keyboard, lv_color_hex(0x2C2C2Eu), LV_PART_ITEMS);
  lv_obj_set_style_text_color(bleMacFormObj.keyboard, lv_color_hex(kColText), LV_PART_ITEMS);
  lv_keyboard_set_mode(bleMacFormObj.keyboard, LV_KEYBOARD_MODE_TEXT_UPPER);
  lv_keyboard_set_textarea(bleMacFormObj.keyboard, bleMacFormObj.ta_room);

  lv_obj_add_event_cb(bleMacFormObj.ta_room, onFieldFocus, LV_EVENT_FOCUSED,
                      bleMacFormObj.keyboard);
  lv_obj_add_event_cb(bleMacFormObj.ta_room, onFieldFocus, LV_EVENT_CLICKED,
                      bleMacFormObj.keyboard);
  lv_obj_add_event_cb(bleMacFormObj.ta_out, onFieldFocus, LV_EVENT_FOCUSED,
                      bleMacFormObj.keyboard);
  lv_obj_add_event_cb(bleMacFormObj.ta_out, onFieldFocus, LV_EVENT_CLICKED,
                      bleMacFormObj.keyboard);

  uiBleMacFormPrepare();
}

void uiBleMacFormPrepare(void) {
  if (!bleMacFormObj.ta_room || !bleMacFormObj.ta_out) {
    return;
  }
  char room[H2_MAC_STR_LEN];
  char out[H2_MAC_STR_LEN];
  climateRoomGetConfiguredMac(room, sizeof(room));
  climateRoomGetConfiguredOutdoorMac(out, sizeof(out));
  if (strcmp(room, "---") == 0 || strcmp(room, "—") == 0) {
    room[0] = '\0';
  }
  if (strcmp(out, "---") == 0 || strcmp(out, "—") == 0) {
    out[0] = '\0';
  }
  lv_textarea_set_text(bleMacFormObj.ta_room, room);
  lv_textarea_set_text(bleMacFormObj.ta_out, out);
  setHint("Formát AA:BB:CC:DD:EE:FF", kColMuted);
  if (bleMacFormObj.keyboard) {
    lv_keyboard_set_textarea(bleMacFormObj.keyboard, bleMacFormObj.ta_room);
  }
}

void uiBleMacFormTick(void) {}

lv_obj_t* uiBleMacFormScreen(void) {
  return bleMacFormObj.screen;
}
