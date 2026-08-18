#include "ui_eez_signal_leds.h"
#include "ui_eez_nav.h"
#include "ui_eez_model.h"
#include "ui_eez_screens.h"
#include "lg_lvgl.h"

namespace {

constexpr int kLedSize = 18;
// Stejná zelená jako Wi‑Fi label na hlavní obrazovce (0x30d158)
constexpr uint32_t kOnGreen = 0x30D158u;
constexpr uint32_t kOffColor = 0xAEAEB2u;
constexpr uint32_t kOnOrange = 0xFF9F0Au;  // odmrazování + el. topení
constexpr uint32_t kOnPurple = 0xBF5AF2u;  // MQTT watch — oko
constexpr uint32_t kMuted = 0x8E8E93u;
constexpr uint32_t kBtnBg = 0x1A1A1Fu;
constexpr uint32_t kBtnBgOn = 0x14301Cu;

constexpr int kEyeX = 710;
constexpr int kEyeY = 14;

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

lv_obj_t* s_lblEye = nullptr;
// Ploché tečky — lv_led stmavuje barvu (ne jako Wi‑Fi text)
lv_obj_t* s_dots[5] = {};

static const SignalLedDef* signalDefs(size_t* countOut) {
  static const SignalLedDef defs[] = {
      {ledChod, lblChod, &UiEezModel::sig_chod, kOnGreen, 608, 181},
      {ledCerpadlo, lblCerpadlo, &UiEezModel::sig_cerpadlo, kOnGreen, 608, 223},
      {ledKompresor, lblKompresor, &UiEezModel::sig_kompresor, kOnGreen, 608, 265},
      {ledOdmrazovani, lblOdmrazovani, &UiEezModel::sig_odmrazovani, kOnOrange, 608, 307},
      {ledElTopeni, lblElTopeni, &UiEezModel::sig_el_topeni, kOnOrange, 608, 349},
  };
  *countOut = sizeof(defs) / sizeof(defs[0]);
  return defs;
}

static void styleDot(lv_obj_t* dot, bool on, uint32_t onColor) {
  if (!dot) {
    return;
  }
  const uint32_t col = on ? onColor : kOffColor;
  lv_obj_set_style_bg_color(dot, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(dot, on ? 10 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_spread(dot, on ? 2 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_color(dot, lv_color_hex(onColor),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_opa(dot, on ? LV_OPA_50 : LV_OPA_TRANSP,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
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

static void ensureEyeWidget() {
  if (!objects.main || s_lblEye) {
    return;
  }
  s_lblEye = lv_label_create(objects.main);
  lv_obj_set_pos(s_lblEye, kEyeX, kEyeY);
  lv_obj_set_style_text_font(s_lblEye, &lv_font_montserrat_28,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(s_lblEye, LV_TEXT_ALIGN_LEFT,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(s_lblEye, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_text(s_lblEye, LV_SYMBOL_EYE_CLOSE);
  lv_obj_set_style_text_color(s_lblEye, lv_color_hex(kMuted),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_move_foreground(s_lblEye);
}

static void applyEye(bool watching) {
  if (!s_lblEye) {
    return;
  }
  static int8_t s_last = -1;
  if (s_last == (int8_t)watching) {
    return;
  }
  s_last = (int8_t)watching;
  lv_label_set_text(s_lblEye,
                    watching ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
  lv_obj_set_style_text_color(s_lblEye,
                              lv_color_hex(watching ? kOnPurple : kMuted),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

}  // namespace

void uiEezInitSignalLeds(void) {
  if (!objects.main) {
    return;
  }

  ensureEyeWidget();

  size_t count = 0;
  const SignalLedDef* defs = signalDefs(&count);
  for (size_t i = 0; i < count; ++i) {
    const SignalLedDef& def = defs[i];
    lv_obj_t* led = def.getLed();
    lv_obj_t* lbl = def.getLbl();

    // EEZ lv_led schovat — kreslí tmavší zelenou než Wi‑Fi text
    if (led) {
      lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
    }

    if (!s_dots[i]) {
      s_dots[i] = lv_obj_create(objects.main);
      lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_pad_all(s_dots[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_border_width(s_dots[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_grad_dir(s_dots[i], LV_GRAD_DIR_NONE,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_pos(s_dots[i], def.x, def.y);
    lv_obj_set_size(s_dots[i], kLedSize, kLedSize);
    lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_dots[i]);
    styleDot(s_dots[i], false, def.onColor);

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
    if (!s_dots[i]) {
      continue;
    }
    const bool on = uiEez.*def.sig;
    if (s_haveLast && s_last[i] == on) {
      continue;
    }
    s_last[i] = on;
    styleDot(s_dots[i], on, def.onColor);
  }
  s_haveLast = true;

  applyStartButton(uiEez.sig_chod);
  applyEye(uiEez.sig_remote);
}
