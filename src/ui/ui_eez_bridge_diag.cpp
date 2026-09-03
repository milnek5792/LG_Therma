#include "ui_eez_bridge_diag.h"

#include "lg_board.h"
#include "app_cmd.h"
#include "climate_room_uart.h"
#include "src/ui_eez_actions.h"
#include "src/ui_eez_fonts.h"
#include "src/ui_eez_model.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

bridge_diag_objects_t bridgeDiagObj;

namespace {

constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColPurple = 0x5856D6u;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kModalW = 420;
constexpr int kModalH = 360;
constexpr int kPad = 14;
constexpr int kBtnH = 48;
constexpr int kLine = 28;

const lv_font_t* kFont = &ui_font_font_cs_24;
bool s_created = false;

void setLabelIfChanged(lv_obj_t* lbl, const char* text) {
  if (!lbl || !text) {
    return;
  }
  const char* cur = lv_label_get_text(lbl);
  if (cur && strcmp(cur, text) == 0) {
    return;
  }
  lv_label_set_text(lbl, text);
}

lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                     lv_event_cb_t cb, uint32_t bg) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lbl);
  return btn;
}

bool pointInObj(lv_obj_t* obj, int tx, int ty) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
    return false;
  }
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return tx >= a.x1 && tx <= a.x2 && ty >= a.y1 && ty <= a.y2;
}

void formatAge(char* buf, size_t len, uint32_t ageMs) {
  if (ageMs == UINT32_MAX) {
    snprintf(buf, len, "—");
    return;
  }
  if (ageMs < 2000u) {
    snprintf(buf, len, "teď");
  } else if (ageMs < 60000u) {
    snprintf(buf, len, "před %u s", (unsigned)(ageMs / 1000u));
  } else {
    snprintf(buf, len, "před %u min", (unsigned)(ageMs / 60000u));
  }
}

void onBgClick(lv_event_t* e) {
  (void)e;
  uiBridgeDiagHide();
}

void onPanelClick(lv_event_t* e) {
  (void)e;
}

void onRefresh(lv_event_t* e) {
  (void)e;
  climateRoomRequestBridgeInfo();
}

void onOta(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_SETTINGS_BRIDGE_OTA);
}

void onClose(lv_event_t* e) {
  (void)e;
  uiBridgeDiagHide();
}

void refreshLabels(void) {
  char line[96];
  char age[24];

  if (climateRoomBridgeInfoOk()) {
    snprintf(line, sizeof(line), "MAC: %s", climateRoomBridgeMac());
  } else {
    snprintf(line, sizeof(line), "MAC: --- (Obnovit)");
  }
  setLabelIfChanged(bridgeDiagObj.lbl_mac, line);

  snprintf(line, sizeof(line), "Kanál ESP-NOW: %u",
           (unsigned)climateRoomBridgeChannel());
  setLabelIfChanged(bridgeDiagObj.lbl_ch, line);

  formatAge(age, sizeof(age), climateRoomBridgeInfoAgeMs());
  snprintf(line, sizeof(line), "ESP-NOW: %s  (%s)",
           climateRoomBridgeEspNowOk() ? "OK" : "—", age);
  setLabelIfChanged(bridgeDiagObj.lbl_espnow, line);

  if (climateRoomLastPwrOk()) {
    formatAge(age, sizeof(age), climateRoomLastPwrAgeMs());
    snprintf(line, sizeof(line), "PWR: %u W · %.3f kWh  (%s)",
             (unsigned)climateRoomLastPwrW(),
             (double)climateRoomLastPwrKwh(), age);
  } else {
    snprintf(line, sizeof(line), "PWR: --- (čekám na vzorek)");
  }
  setLabelIfChanged(bridgeDiagObj.lbl_pwr, line);

  const ClimateBridgeOtaState ota = climateRoomBridgeOtaState();
  if (ota == CLIMATE_BRIDGE_OTA_READY) {
    snprintf(line, sizeof(line), "OTA: %s  %s", climateRoomBridgeOtaHost(),
             climateRoomBridgeOtaIp());
  } else if (ota == CLIMATE_BRIDGE_OTA_CONNECTING) {
    snprintf(line, sizeof(line), "OTA: připojuji Wi-Fi...");
  } else if (ota == CLIMATE_BRIDGE_OTA_FAIL) {
    snprintf(line, sizeof(line), "OTA: selhalo");
  } else {
    snprintf(line, sizeof(line), "OTA: vypnuto");
  }
  setLabelIfChanged(bridgeDiagObj.lbl_ota, line);
}

}  // namespace

void uiBridgeDiagCreate(void) {
  if (s_created) {
    return;
  }
  s_created = true;
  memset(&bridgeDiagObj, 0, sizeof(bridgeDiagObj));

  bridgeDiagObj.modal_bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bridgeDiagObj.modal_bg, kW, kH);
  lv_obj_set_pos(bridgeDiagObj.modal_bg, 0, 0);
  lv_obj_set_style_bg_color(bridgeDiagObj.modal_bg, lv_color_hex(0x000000u),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(bridgeDiagObj.modal_bg, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(bridgeDiagObj.modal_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(bridgeDiagObj.modal_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bridgeDiagObj.modal_bg, onBgClick, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_HIDDEN);

  const int mx = (kW - kModalW) / 2;
  const int my = (kH - kModalH) / 2;
  bridgeDiagObj.modal_panel = makePanel(bridgeDiagObj.modal_bg, mx, my, kModalW, kModalH);
  lv_obj_add_flag(bridgeDiagObj.modal_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bridgeDiagObj.modal_panel, onPanelClick, LV_EVENT_CLICKED, nullptr);

  auto makeLbl = [](int y, const char* text, uint32_t col) {
    lv_obj_t* lbl = lv_label_create(bridgeDiagObj.modal_panel);
    lv_obj_set_pos(lbl, kPad, y);
    lv_obj_set_width(lbl, kModalW - 2 * kPad);
    lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl, text);
    return lbl;
  };

  bridgeDiagObj.lbl_title = makeLbl(12, "Bridge diagnostika", kColOrange);
  lv_obj_set_style_text_align(bridgeDiagObj.lbl_title, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  bridgeDiagObj.lbl_mac = makeLbl(52, "MAC: ---", kColText);
  bridgeDiagObj.lbl_ch = makeLbl(52 + kLine, "Kanál ESP-NOW: ---", kColMuted);
  bridgeDiagObj.lbl_espnow = makeLbl(52 + 2 * kLine, "ESP-NOW: ---", kColMuted);
  bridgeDiagObj.lbl_pwr = makeLbl(52 + 3 * kLine, "PWR: ---", kColMuted);
  bridgeDiagObj.lbl_ota = makeLbl(52 + 4 * kLine, "OTA: ---", kColMuted);

  const int btnY = kModalH - kBtnH - kPad;
  const int btnW = (kModalW - 2 * kPad - 2 * 10) / 3;
  bridgeDiagObj.btn_refresh =
      makeButton(bridgeDiagObj.modal_panel, kPad, btnY, btnW, kBtnH, "Obnovit",
                 onRefresh, kColAccent);
  bridgeDiagObj.btn_ota = makeButton(bridgeDiagObj.modal_panel,
                                     kPad + btnW + 10, btnY, btnW, kBtnH, "OTA",
                                     onOta, kColGreen);
  bridgeDiagObj.btn_close =
      makeButton(bridgeDiagObj.modal_panel, kPad + 2 * (btnW + 10), btnY, btnW,
                 kBtnH, "Zavřít", onClose, kColPurple);
}

void uiBridgeDiagShow(void) {
  uiBridgeDiagCreate();
  if (!bridgeDiagObj.modal_bg) {
    return;
  }
  climateRoomRequestBridgeInfo();
  refreshLabels();
  lv_obj_remove_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(bridgeDiagObj.modal_bg);
}

void uiBridgeDiagHide(void) {
  if (bridgeDiagObj.modal_bg) {
    lv_obj_add_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
  }
}

bool uiBridgeDiagIsOpen(void) {
  return bridgeDiagObj.modal_bg &&
         !lv_obj_has_flag(bridgeDiagObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
}

void uiBridgeDiagTick(void) {
  if (!uiBridgeDiagIsOpen()) {
    return;
  }
  refreshLabels();
}

lv_obj_t* uiBridgeDiagHitTest(int tx, int ty) {
  if (!uiBridgeDiagIsOpen()) {
    return nullptr;
  }
  lv_obj_t* list[] = {
      bridgeDiagObj.btn_refresh, bridgeDiagObj.btn_ota, bridgeDiagObj.btn_close,
      bridgeDiagObj.modal_panel, bridgeDiagObj.modal_bg,
  };
  for (int i = 0; i < (int)(sizeof(list) / sizeof(list[0])); ++i) {
    if (pointInObj(list[i], tx, ty)) {
      return list[i];
    }
  }
  return nullptr;
}
