#include "src/ui_eez_wifi_form.h"

#include "app_cmd.h"
#include "src/net_wifi_mgr.h"
#include "storage_config_nvs.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"
#include "ui_eez_nav.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

wifi_form_objects_t wifiFormObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColGreen = 0x30D158u;

const lv_font_t* kFontTitle = &ui_font_font_cs_24;
const lv_font_t* kFontBody = &ui_font_font_cs_24;

void styleField(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(obj, kFontBody, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void styleActionBtn(lv_obj_t* obj, uint32_t bg) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(obj, 0);
  if (lbl) {
    lv_obj_set_style_text_font(lbl, kFontBody, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void readForm(char* ssid, size_t ssidLen, char* pass, size_t passLen) {
  const char* ssidText = lv_textarea_get_text(wifiFormObj.ta_ssid);
  const char* passText = lv_textarea_get_text(wifiFormObj.ta_pass);
  strncpy(ssid, ssidText ? ssidText : "", ssidLen - 1);
  ssid[ssidLen - 1] = '\0';
  strncpy(pass, passText ? passText : "", passLen - 1);
  pass[passLen - 1] = '\0';
}

void onBack(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_WIFI_FORM_BACK);
}

void onSave(lv_event_t* e) {
  (void)e;
  if (!uiWifiFormSaveCredentials()) {
    return;
  }
  appCmdEnqueueHmi(UI_AKCE_WIFI_FORM_SAVE);
}

void onConnect(lv_event_t* e) {
  (void)e;
  if (!uiWifiFormSaveCredentials()) {
    return;
  }
  appCmdEnqueueHmi(UI_AKCE_WIFI_FORM_CONNECT);
}

void onFieldFocus(lv_event_t* e) {
  lv_obj_t* kb = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
  lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!kb || !ta) { return; }
  lv_keyboard_set_textarea(kb, ta);
}

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, const char* text) {
  lv_obj_t* obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_font(obj, kFontBody, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(kColMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
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
  lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  return obj;
}

}  // namespace

bool uiWifiFormSaveCredentials() {
  if (!wifiFormObj.ta_ssid || !wifiFormObj.ta_pass) {
    return false;
  }
  char ssid[33];
  char pass[65];
  const char* ssidText = lv_textarea_get_text(wifiFormObj.ta_ssid);
  const char* passText = lv_textarea_get_text(wifiFormObj.ta_pass);
  strncpy(ssid, ssidText ? ssidText : "", sizeof(ssid) - 1);
  ssid[sizeof(ssid) - 1] = '\0';
  strncpy(pass, passText ? passText : "", sizeof(pass) - 1);
  pass[sizeof(pass) - 1] = '\0';
  if (ssid[0] == '\0') {
    Serial.println("[NET] Wi-Fi: prazdne SSID");
    return false;
  }
  netWifiSetCredentials(ssid, pass);
  Serial.printf("[NET] Wi-Fi ulozeno: %s\n", ssid);
  return true;
}

void uiWifiFormCreate() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  wifiFormObj.screen = scr;
  lv_obj_set_size(scr, 1280, 720);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  wifiFormObj.btn_back = makeActionButton(
      scr, 40, 16, 220, 52, "<- ZPET", onBack, 0x48484Fu);

  wifiFormObj.lbl_title = lv_label_create(scr);
  lv_label_set_text(wifiFormObj.lbl_title, "Wi-Fi sit");
  lv_obj_set_style_text_font(wifiFormObj.lbl_title, kFontTitle, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(wifiFormObj.lbl_title, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_align(wifiFormObj.lbl_title, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_y(wifiFormObj.lbl_title, 24);

  wifiFormObj.lbl_ssid = makeLabel(scr, 40, 88, "SSID site");
  wifiFormObj.ta_ssid = lv_textarea_create(scr);
  lv_obj_set_pos(wifiFormObj.ta_ssid, 40, 120);
  lv_obj_set_size(wifiFormObj.ta_ssid, 1200, 52);
  lv_textarea_set_one_line(wifiFormObj.ta_ssid, true);
  lv_textarea_set_max_length(wifiFormObj.ta_ssid, 32);
  styleField(wifiFormObj.ta_ssid);

  wifiFormObj.lbl_pass = makeLabel(scr, 40, 184, "Heslo");
  wifiFormObj.ta_pass = lv_textarea_create(scr);
  lv_obj_set_pos(wifiFormObj.ta_pass, 40, 216);
  lv_obj_set_size(wifiFormObj.ta_pass, 1200, 52);
  lv_textarea_set_one_line(wifiFormObj.ta_pass, true);
  lv_textarea_set_max_length(wifiFormObj.ta_pass, 64);
  styleField(wifiFormObj.ta_pass);

  wifiFormObj.btn_save = makeActionButton(
      scr, 40, 288, 300, 52, "Ulozit", onSave, 0x48484Fu);
  wifiFormObj.btn_connect = makeActionButton(
      scr, 360, 288, 360, 52, "Ulozit a pripojit", onConnect, kColGreen);

  wifiFormObj.keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(wifiFormObj.keyboard, 1280, 360);
  lv_obj_align(wifiFormObj.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(wifiFormObj.keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(wifiFormObj.keyboard, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(wifiFormObj.keyboard, lv_color_hex(0x2C2C2Eu), LV_PART_ITEMS);
  lv_obj_set_style_text_color(wifiFormObj.keyboard, lv_color_hex(kColText), LV_PART_ITEMS);
  lv_keyboard_set_mode(wifiFormObj.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(wifiFormObj.keyboard, wifiFormObj.ta_ssid);

  lv_obj_add_event_cb(wifiFormObj.ta_ssid, onFieldFocus, LV_EVENT_FOCUSED, wifiFormObj.keyboard);
  lv_obj_add_event_cb(wifiFormObj.ta_ssid, onFieldFocus, LV_EVENT_CLICKED, wifiFormObj.keyboard);
  lv_obj_add_event_cb(wifiFormObj.ta_pass, onFieldFocus, LV_EVENT_FOCUSED, wifiFormObj.keyboard);
  lv_obj_add_event_cb(wifiFormObj.ta_pass, onFieldFocus, LV_EVENT_CLICKED, wifiFormObj.keyboard);

  uiWifiFormPrepare();
}

void uiWifiFormPrepare() {
  if (!wifiFormObj.ta_ssid || !wifiFormObj.ta_pass) { return; }

  char ssid[33] = "";
  char pass[65] = "";
  if (!storageLoadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
    ssid[0] = '\0';
    pass[0] = '\0';
  }

  lv_textarea_set_text(wifiFormObj.ta_ssid, ssid);
  lv_textarea_set_text(wifiFormObj.ta_pass, pass);
  if (wifiFormObj.keyboard) {
    lv_keyboard_set_textarea(wifiFormObj.keyboard, wifiFormObj.ta_ssid);
  }
}

void uiWifiFormTick() {}

lv_obj_t* uiWifiFormScreen() {
  return wifiFormObj.screen;
}
