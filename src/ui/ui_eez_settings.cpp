#include "ui_eez_settings.h"

#include "board_7b.h"
#include "ui_display_mgr.h"
#include "ui_eez_actions.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

settings_objects_t settingsObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColDisabled = 0x2C2C2Eu;
constexpr uint32_t kColPurple = 0x5856D6u;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 10;
constexpr int kGap = 10;
constexpr int kHeaderH = 44;
constexpr int kBtnH = 34;
constexpr int kLine = 26;
constexpr int kPad = 10;
constexpr int kTitleY = 6;
constexpr int kBodyY = 40;

const lv_font_t* kFont = &ui_font_font_cs_16;

void setLabelIfChanged(lv_obj_t* lbl, const char* text) {
  if (!lbl || !text) {
    return;
  }
  const char* prev = lv_label_get_text(lbl);
  if (prev && strcmp(prev, text) == 0) {
    return;
  }
  lv_label_set_text(lbl, text);
}

lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, int maxW, const char* text, uint32_t color) {
  lv_obj_t* obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  if (maxW > 0) {
    lv_obj_set_width(obj, maxW);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
  }
  lv_obj_set_style_text_font(obj, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(obj, text);
  return obj;
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text, lv_event_cb_t cb, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(obj);
  lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

void styleToggleBtn(lv_obj_t* btn, bool on) {
  if (!btn) {
    return;
  }
  const void* prev = lv_obj_get_user_data(btn);
  const uintptr_t state = on ? 1u : 2u;
  if (reinterpret_cast<uintptr_t>(prev) == state) {
    return;
  }
  lv_obj_set_user_data(btn, reinterpret_cast<void*>(state));
  lv_obj_set_style_bg_color(
      btn, lv_color_hex(on ? kColGreen : 0x48484Au), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, on ? "Zapnuto" : "Vypnuto");
  }
}

void styleConnectBtn(lv_obj_t* btn, bool enabled, bool connected) {
  if (!btn) {
    return;
  }
  const uintptr_t state = connected ? 3u : (enabled ? 2u : 1u);
  const void* prev = lv_obj_get_user_data(btn);
  if (reinterpret_cast<uintptr_t>(prev) == state) {
    return;
  }
  lv_obj_set_user_data(btn, reinterpret_cast<void*>(state));
  const uint32_t bg = connected ? kColGreen : (enabled ? kColAccent : kColDisabled);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, (enabled || connected) ? 255 : 180, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, connected ? "MQTT OK" : "Pripojit MQTT");
  }
}

void setTextColorCached(lv_obj_t* obj, uint32_t color) {
  if (!obj) {
    return;
  }
  const uintptr_t want = color;
  if (reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj)) == want) {
    return;
  }
  lv_obj_set_user_data(obj, reinterpret_cast<void*>(want));
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void updateBrightnessLabel() {
  if (!settingsObj.lbl_brightness) {
    return;
  }
  char line[24];
  snprintf(line, sizeof(line), "Jas %u%%", (unsigned)uiDisplayGetBrightness());
  setLabelIfChanged(settingsObj.lbl_brightness, line);
}

void updateSleepLabels() {
  if (settingsObj.btn_sleep) {
    lv_obj_t* lbl = lv_obj_get_child(settingsObj.btn_sleep, 0);
    if (lbl) {
      char line[32];
      snprintf(line, sizeof(line), "Usinani: %s", uiDisplaySleepTimeoutLabel());
      setLabelIfChanged(lbl, line);
    }
  }
}

void onBrightnessChanged(lv_event_t* e) {
  lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
  const int32_t v = lv_slider_get_value(slider);
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    uiDisplaySetBrightness(static_cast<uint8_t>(v), false);
    updateBrightnessLabel();
  } else if (code == LV_EVENT_RELEASED) {
    uiDisplaySetBrightness(static_cast<uint8_t>(v), true);
    updateBrightnessLabel();
  }
}

void onSleepCycle(lv_event_t* e) {
  (void)e;
  uiDisplayCycleSleepTimeout();
  updateSleepLabels();
}

}  // namespace

void uiSettingsCreate() {
  memset(&settingsObj, 0, sizeof(settingsObj));

  lv_obj_t* scr = lv_obj_create(nullptr);
  settingsObj.screen = scr;
  lv_obj_set_size(scr, kW, kH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  const int contentW = kW - 2 * kMargin;
  const int colW = (contentW - kGap) / 2;
  const int toggleW = 110;
  const int btnW = (colW - 2 * kPad - kGap) / 2;
  const int labelW = colW - 2 * kPad;
  const int titleW = colW - toggleW - 3 * kPad;

  settingsObj.btn_back = makeButton(
      scr, kMargin, 4, 120, kBtnH, "<- ZPET", action_akce_zpet, 0x48484Fu);
  settingsObj.lbl_title = makeLabel(scr, 0, 10, 0, "NASTAVENI", kColText);
  lv_obj_set_style_align(settingsObj.lbl_title, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- levý sloupec: Wi-Fi ---
  const int topY = kHeaderH;
  const int topH = 200;
  settingsObj.panel_wifi = makePanel(scr, kMargin, topY, colW, topH);
  settingsObj.lbl_wifi_title =
      makeLabel(settingsObj.panel_wifi, kPad, kTitleY, titleW, "Wi-Fi", kColOrange);
  settingsObj.lbl_wifi_status =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY, labelW, "Stav: ---", kColText);
  settingsObj.lbl_wifi_ssid =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY + kLine, labelW, "Sit: ---", kColMuted);
  settingsObj.lbl_wifi_ip =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY + 2 * kLine, labelW, "IP: ---", kColMuted);
  settingsObj.btn_wifi_toggle = makeButton(
      settingsObj.panel_wifi, colW - toggleW - kPad, kTitleY, toggleW, kBtnH,
      "Vypnuto", action_akce_wifi_toggle, kColAccent);
  const int wifiBtnY = topH - kBtnH - kPad;
  settingsObj.btn_wifi_connect = makeButton(
      settingsObj.panel_wifi, kPad, wifiBtnY, btnW, kBtnH,
      "Pripojit", action_akce_wifi_connect, kColAccent);
  settingsObj.btn_wifi_edit = makeButton(
      settingsObj.panel_wifi, kPad + btnW + kGap, wifiBtnY, btnW, kBtnH,
      "Nastavit sit", action_akce_wifi_edit, kColPurple);

  // --- pravý sloupec: MQTT ---
  settingsObj.panel_mqtt = makePanel(scr, kMargin + colW + kGap, topY, colW, topH);
  settingsObj.lbl_mqtt_title =
      makeLabel(settingsObj.panel_mqtt, kPad, kTitleY, titleW, "MQTT", kColOrange);
  settingsObj.lbl_mqtt_status =
      makeLabel(settingsObj.panel_mqtt, kPad, kBodyY, labelW, "Stav: ---", kColText);
  settingsObj.lbl_mqtt_host =
      makeLabel(settingsObj.panel_mqtt, kPad, kBodyY + kLine, labelW, "Broker: ---", kColMuted);
  settingsObj.btn_mqtt_toggle = makeButton(
      settingsObj.panel_mqtt, colW - toggleW - kPad, kTitleY, toggleW, kBtnH,
      "Vypnuto", action_akce_mqtt_toggle, kColAccent);
  settingsObj.btn_mqtt_connect = makeButton(
      settingsObj.panel_mqtt, kPad, topH - kBtnH - kPad, 180, kBtnH,
      "Pripojit MQTT", action_akce_mqtt_connect, kColAccent);

  // --- Displej: jeden úzký řádek ---
  const int dispY = topY + topH + kGap;
  const int dispH = 52;
  settingsObj.panel_display = makePanel(scr, kMargin, dispY, contentW, dispH);
  settingsObj.lbl_disp_title =
      makeLabel(settingsObj.panel_display, kPad, 14, 70, "Displej", kColOrange);
  settingsObj.lbl_brightness =
      makeLabel(settingsObj.panel_display, kPad + 80, 14, 90, "Jas 60%", kColText);

  const int sliderX = kPad + 170;
  const int sleepBtnW = 170;
  const int sliderW = contentW - sliderX - sleepBtnW - 2 * kPad - kGap;
  settingsObj.slider_brightness = lv_slider_create(settingsObj.panel_display);
  lv_obj_set_pos(settingsObj.slider_brightness, sliderX, 16);
  lv_obj_set_size(settingsObj.slider_brightness, sliderW, 20);
  lv_slider_set_range(settingsObj.slider_brightness, 10, 97);
  lv_slider_set_value(settingsObj.slider_brightness, uiDisplayGetBrightness(), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(0x3A3A3Cu),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(kColAccent),
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(0xFFFFFFu),
                            LV_PART_KNOB | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(settingsObj.slider_brightness, onBrightnessChanged,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(settingsObj.slider_brightness, onBrightnessChanged, LV_EVENT_RELEASED,
                      nullptr);

  settingsObj.lbl_sleep = nullptr;
  settingsObj.btn_sleep = makeButton(
      settingsObj.panel_display, contentW - kPad - sleepBtnW, 8, sleepBtnW, kBtnH,
      "Usinani: 2 min", onSleepCycle, kColPurple);

  // --- SwitchBot + další (plná šířka) ---
  const int sysY = dispY + dispH + kGap;
  const int sysH = kH - sysY - kMargin;
  settingsObj.panel_sys = makePanel(scr, kMargin, sysY, contentW, sysH);
  settingsObj.lbl_sys_title =
      makeLabel(settingsObj.panel_sys, kPad, kTitleY, 200, "SwitchBot", kColOrange);
  settingsObj.lbl_sys_hint =
      makeLabel(settingsObj.panel_sys, kPad + 140, kTitleY + 2,
                contentW - 2 * kPad - 140, "Cekam na senzor...", kColMuted);

  const int rowY = sysH - kBtnH - kPad;
  const int threeW = (contentW - 2 * kPad - 2 * kGap) / 3;
  settingsObj.btn_ble = makeButton(
      settingsObj.panel_sys, kPad, rowY, threeW, kBtnH,
      "Nacist ted", action_akce_settings_ble, kColPurple);
  settingsObj.btn_plan = makeButton(
      settingsObj.panel_sys, kPad + threeW + kGap, rowY, threeW, kBtnH,
      "Casovy plan", action_akce_settings_plan, kColPurple);
  settingsObj.btn_servis = makeButton(
      settingsObj.panel_sys, kPad + 2 * (threeW + kGap), rowY, threeW, kBtnH,
      "Servis", action_akce_settings_servis, kColPurple);

  strncpy(uiEez.set_sys_hint, "Cekam na senzor...", sizeof(uiEez.set_sys_hint) - 1);
  uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';

  updateBrightnessLabel();
  updateSleepLabels();
  uiSettingsTick();
}

void uiSettingsTick() {
  if (!settingsObj.screen) {
    return;
  }

  char line[80];

  snprintf(line, sizeof(line), "Stav: %s", uiEez.set_wifi_status);
  setLabelIfChanged(settingsObj.lbl_wifi_status, line);
  {
    uint32_t col = kColText;
    if (uiEez.sig_wifi) {
      col = kColGreen;
    } else if (strstr(uiEez.set_wifi_status, "Pripoj") != nullptr) {
      col = kColOrange;
    }
    setTextColorCached(settingsObj.lbl_wifi_status, col);
  }

  snprintf(line, sizeof(line), "Sit: %s", uiEez.set_wifi_ssid);
  setLabelIfChanged(settingsObj.lbl_wifi_ssid, line);

  snprintf(line, sizeof(line), "IP: %s", uiEez.set_wifi_ip);
  setLabelIfChanged(settingsObj.lbl_wifi_ip, line);

  snprintf(line, sizeof(line), "Stav: %s", uiEez.set_mqtt_status);
  setLabelIfChanged(settingsObj.lbl_mqtt_status, line);
  {
    uint32_t col = kColText;
    if (uiEez.sig_mqtt) {
      col = kColGreen;
    } else if (strstr(uiEez.set_mqtt_status, "Pripoj") != nullptr) {
      col = kColOrange;
    }
    setTextColorCached(settingsObj.lbl_mqtt_status, col);
  }

  snprintf(line, sizeof(line), "Broker: %s", uiEez.set_mqtt_host);
  setLabelIfChanged(settingsObj.lbl_mqtt_host, line);

  setLabelIfChanged(settingsObj.lbl_sys_hint, uiEez.set_sys_hint);
  {
    uint32_t col = kColMuted;
    if (uiEez.sig_ble) {
      col = kColGreen;
    } else if (strstr(uiEez.set_sys_hint, "Skenuji") != nullptr) {
      col = kColOrange;
    }
    setTextColorCached(settingsObj.lbl_sys_hint, col);
  }

  styleToggleBtn(settingsObj.btn_wifi_toggle, uiEez.set_wifi_enabled);
  styleToggleBtn(settingsObj.btn_mqtt_toggle, uiEez.set_mqtt_enabled);

  const bool mqttReady = uiEez.set_wifi_enabled && uiEez.sig_wifi;
  styleConnectBtn(settingsObj.btn_mqtt_connect, mqttReady, uiEez.sig_mqtt);
}

lv_obj_t* uiSettingsScreen() {
  return settingsObj.screen;
}
