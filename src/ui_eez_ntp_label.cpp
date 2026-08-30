#include "ui_eez_ntp_label.h"

#include "net_ntp_time.h"
#include "ui_eez_fonts.h"
#include "ui_eez_nav.h"
#include "ui_eez_screens.h"

#include <string.h>

namespace {

lv_obj_t* s_lblNtp = nullptr;
char s_lastText[16] = "";

constexpr uint32_t kColOk = 0x30D158u;
constexpr uint32_t kColWait = 0xFF9F0Au;
constexpr uint32_t kColOff = 0x8E8E93u;

const char* ntpLabelText() {
  if (netNtpIsSynced()) {
    return "NTP: OK";
  }
  if (netNtpIsWaiting()) {
    return "NTP: ...";
  }
  return "NTP: ---";
}

uint32_t ntpLabelColor() {
  if (netNtpIsSynced()) {
    return kColOk;
  }
  if (netNtpIsWaiting()) {
    return kColWait;
  }
  return kColOff;
}

}  // namespace

void uiEezNtpLabelInit(void) {
  if (!objects.main || s_lblNtp) {
    return;
  }

  s_lblNtp = lv_label_create(objects.main);
  lv_obj_set_pos(s_lblNtp, 380, 15);
  lv_obj_set_style_text_font(s_lblNtp, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(s_lblNtp, lv_color_hex(kColOff), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(s_lblNtp, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(s_lblNtp, "NTP: ---");
  strncpy(s_lastText, "NTP: ---", sizeof(s_lastText));
  s_lastText[sizeof(s_lastText) - 1] = '\0';
}

void uiEezNtpLabelTick(void) {
  if (!s_lblNtp || !uiIsMainScreen()) {
    return;
  }

  const char* text = ntpLabelText();
  const uint32_t color = ntpLabelColor();

  if (strcmp(text, s_lastText) != 0) {
    lv_label_set_text(s_lblNtp, text);
    strncpy(s_lastText, text, sizeof(s_lastText));
    s_lastText[sizeof(s_lastText) - 1] = '\0';
  }

  lv_obj_set_style_text_color(
      s_lblNtp, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}
