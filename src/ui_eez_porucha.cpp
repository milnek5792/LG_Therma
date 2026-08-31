#include "ui_eez_porucha.h"

#include "bus_lg_model.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"
#include "ui_eez_nav.h"
#include "ui_eez_screens.h"

#include <string.h>

namespace {

lv_obj_t* s_panel = nullptr;
lv_obj_t* s_lblTitle = nullptr;
lv_obj_t* s_lblText = nullptr;
char s_lastText[80] = "";
bool s_lastVisible = false;

constexpr uint32_t kColTitle = 0xFF453Au;
constexpr uint32_t kColText = 0xFFD60Au;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;

void setVisible(bool on) {
  if (!s_panel) {
    return;
  }
  if (on == s_lastVisible) {
    return;
  }
  s_lastVisible = on;
  if (on) {
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace

void uiEezRefreshPorucha(void) {
  char msg[sizeof(uiEez.porucha_text)] = "";

  if (!lgMaCerstoA0()) {
    strncpy(msg, "Ztráta spojení s venkovní jednotkou (LIN)", sizeof(msg));
  } else {
    lgModelLock();
    const bool cekaOrig = cekameNaOrigStart;
    lgModelUnlock();

    if (uiEez.rezim != UI_REZIM_AUTO && uiEez.sp_pending != 0 &&
        uiEezTeplotaVodySetColor() == UI_SP_COLOR_WARN) {
      strncpy(msg, "Teplota vody nebyla potvrzena venkovní jednotkou",
              sizeof(msg));
    } else if (cekaOrig) {
      strncpy(msg, "Čekám na spuštění tepelného čerpadla", sizeof(msg));
    }
  }

  msg[sizeof(msg) - 1] = '\0';
  if (strcmp(msg, uiEez.porucha_text) != 0) {
    strncpy(uiEez.porucha_text, msg, sizeof(uiEez.porucha_text));
    uiEez.porucha_text[sizeof(uiEez.porucha_text) - 1] = '\0';
  }
  uiEez.sig_alarm = uiEez.porucha_text[0] != '\0';
}

void uiEezPoruchaInit(void) {
  if (!objects.main || s_panel) {
    return;
  }

  s_panel = lv_obj_create(objects.main);
  lv_obj_set_pos(s_panel, 40, 528);
  lv_obj_set_size(s_panel, 770, 72);
  lv_obj_set_style_pad_all(s_panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(s_panel, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(s_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(s_panel, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(s_panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(s_panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

  s_lblTitle = lv_label_create(s_panel);
  lv_obj_set_pos(s_lblTitle, 4, 0);
  lv_obj_set_style_text_font(s_lblTitle, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(s_lblTitle, lv_color_hex(kColTitle), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text_static(s_lblTitle, "Poruchové hlášení");

  s_lblText = lv_label_create(s_panel);
  lv_obj_set_pos(s_lblText, 4, 30);
  lv_obj_set_width(s_lblText, 740);
  lv_obj_set_style_text_font(s_lblText, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(s_lblText, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_long_mode(s_lblText, LV_LABEL_LONG_WRAP);
  lv_label_set_text(s_lblText, "");
}

void uiEezPoruchaTick(void) {
  if (!s_panel || !uiIsMainScreen()) {
    return;
  }

  const bool show = uiEez.porucha_text[0] != '\0';
  setVisible(show);
  if (!show) {
    return;
  }

  if (strcmp(uiEez.porucha_text, s_lastText) != 0) {
    lv_label_set_text(s_lblText, uiEez.porucha_text);
    strncpy(s_lastText, uiEez.porucha_text, sizeof(s_lastText));
    s_lastText[sizeof(s_lastText) - 1] = '\0';
  }
}
