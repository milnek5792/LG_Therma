#include "ui_eez_signal_leds.h"
#include "ui_eez_nav.h"
#include "ui_eez_model.h"
#include "ui_eez_screens.h"
#include "lg_lvgl.h"

namespace {

// EEZ export 1024×600 — pozice led_* z ui_eez_screens.cpp
constexpr int kLedSize = 22;
constexpr uint32_t kOffColor = 0xAEAEB2u;
// Stejná zelená jako border tlačítka START (ui_eez_screens.cpp)
constexpr uint32_t kOnGreen = 0x30D158u;
constexpr uint32_t kOnYellow = 0xFFD60Au;
constexpr uint32_t kOnOrange = 0xFF9F0Au;
constexpr uint32_t kBtnBg = 0x1A1A1Fu;
constexpr uint32_t kBtnBgOn = 0x14301Cu;  // jemný zelený fill při ON

struct SignalLedDef {
  lv_obj_t* (*getLed)();
  lv_obj_t* (*getLbl)();
  bool UiEezModel::* sig;
  uint32_t onColor;
  int x;
  int y;
};

lv_obj_t* ledChod() { return objects.led_chod; }
lv_obj_t* ledCerpadlo() { return objects.led_cerpadlo; }
lv_obj_t* ledKompresor() { return objects.led_kompresor; }
lv_obj_t* ledOdmrazovani() { return objects.led_odmrazovani; }
lv_obj_t* ledElTopeni() { return objects.led_el_topeni; }

lv_obj_t* lblChod() { return objects.lbl_chod; }
lv_obj_t* lblCerpadlo() { return objects.lbl_cerpadlo; }
lv_obj_t* lblKompresor() { return objects.lbl_kompresor; }
lv_obj_t* lblOdmrazovani() { return objects.lbl_odmrazovani; }
lv_obj_t* lblElTopeni() { return objects.lbl_el_topeni; }

static const SignalLedDef* signalDefs(size_t* countOut) {
  static const SignalLedDef defs[] = {
      {ledChod, lblChod, &UiEezModel::sig_chod, kOnGreen, 608, 181},
      {ledCerpadlo, lblCerpadlo, &UiEezModel::sig_cerpadlo, kOnGreen, 608, 223},
      {ledKompresor, lblKompresor, &UiEezModel::sig_kompresor, kOnOrange, 608, 265},
      {ledOdmrazovani, lblOdmrazovani, &UiEezModel::sig_odmrazovani, kOnYellow, 608, 307},
      {ledElTopeni, lblElTopeni, &UiEezModel::sig_el_topeni, kOnYellow, 608, 349},
  };
  *countOut = sizeof(defs) / sizeof(defs[0]);
  return defs;
}

static void styleLed(lv_obj_t* led, bool on, uint32_t onColor) {
  if (!led) {
    return;
  }
  const uint32_t col = on ? onColor : kOffColor;
  lv_led_set_color(led, lv_color_hex(col));
  lv_led_set_brightness(led, on ? 255 : 220);
  lv_obj_set_style_bg_color(led, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(led, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(led, on ? 8 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_color(led, lv_color_hex(onColor), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_opa(led, on ? LV_OPA_40 : LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void applyStartButton(bool on) {
  lv_obj_t* btn = objects.btn_run;
  if (!btn) {
    return;
  }
  static int8_t s_last = -1;
  if (s_last == (int8_t)on) {
    return;
  }
  s_last = (int8_t)on;

  lv_obj_set_style_bg_color(btn, lv_color_hex(on ? kBtnBgOn : kBtnBg),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(btn, lv_color_hex(kOnGreen),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(btn, on ? 3 : 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

}  // namespace

void uiEezInitSignalLeds(void) {
  if (!objects.main) {
    return;
  }

  size_t count = 0;
  const SignalLedDef* defs = signalDefs(&count);
  for (size_t i = 0; i < count; ++i) {
    const SignalLedDef& def = defs[i];
    lv_obj_t* led = def.getLed();
    lv_obj_t* lbl = def.getLbl();
    if (!led) {
      continue;
    }

    lv_obj_remove_flag(led, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(led, def.x, def.y);
    lv_obj_set_size(led, kLedSize, kLedSize);
    lv_obj_remove_flag(led, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(led);
    styleLed(led, false, def.onColor);

    if (lbl) {
      lv_obj_set_x(lbl, 640);
      lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_move_foreground(lbl);
    }
  }

  uiEezApplySignalLeds();
}

void uiEezApplySignalLeds(void) {
  if (!uiIsMainScreen()) {
    return;
  }
  size_t count = 0;
  const SignalLedDef* defs = signalDefs(&count);
  static bool s_last[8] = {};
  static bool s_haveLast = false;
  for (size_t i = 0; i < count; ++i) {
    const SignalLedDef& def = defs[i];
    lv_obj_t* led = def.getLed();
    if (!led) {
      continue;
    }
    const bool on = uiEez.*def.sig;
    if (s_haveLast && s_last[i] == on) {
      continue;
    }
    s_last[i] = on;
    styleLed(led, on, def.onColor);
  }
  s_haveLast = true;

  applyStartButton(uiEez.sig_chod);
}
