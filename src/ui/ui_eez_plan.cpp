#include "ui_eez_plan.h"

#include "lg_board.h"
#include "climate_plan.h"
#include "src/ui_eez_actions.h"
#include "src/ui_eez_fonts.h"
#include "src/ui_eez_model.h"
#include "src/ui_ui_lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <Arduino.h>

plan_objects_t planObj;

void uiPlanCloseModal(bool commit);
void uiPlanOpenModal(uint8_t obdobi);

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColPurple = 0x5856D6u;
constexpr uint32_t kColRed = 0xFF453Au;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 8;
constexpr int kHeaderH = 64;
constexpr int kBtnH = 54;
constexpr int kPad = 8;
constexpr int kDenW = 72;
constexpr int kColGap = 6;
constexpr int kMinUtlumStupne = 1;
constexpr int kMaxUtlumStupne = 5;
constexpr int kTimeRowH = 92;
constexpr int kCasNameW = 48;
constexpr int kCasColX = kCasNameW + 6;
constexpr int kTableTop = kHeaderH + kTimeRowH + 8;
constexpr int kCasStepMin = 30;
constexpr int kDelkaMinMin = 30;
constexpr int kDelkaMaxVtMin = 4 * 60;
constexpr int kDelkaMaxNocMin = 8 * 60;

constexpr int kModalW = 480;
constexpr int kModalH = 360;
constexpr int kModalBtnH = 52;
constexpr int kModalBtnW = 80;

const lv_font_t* kFont = &ui_font_font_cs_24;

static bool s_planCreated = false;
static bool s_planDirty = false;
static int8_t s_modalObdobi = -1;
static bool s_modalEdited = false;
static PlanObdobiCas s_modalBackup;

struct CellRef {
  uint8_t den;
  uint8_t obdobi;
};

enum class ModalAction : uint8_t {
  OdMinus = 0,
  OdPlus,
  DelMinus,
  DelPlus,
  Hotovo,
};

struct ObdobiRef {
  uint8_t obdobi;
};

static CellRef s_cellRefs[PLAN_POCET_DNU][PLAN_POCET_OBDOBI];
static ObdobiRef s_obdobiRefs[PLAN_POCET_OBDOBI];
static uint32_t s_lastCellClickMs = 0;
static uint16_t s_lastCellKey = 0xFFFFu;
static lv_obj_t* s_modalHint = nullptr;

int maxDelkaProObdobi(uint8_t obdobi) {
  return (obdobi == PLAN_OBDOBI_NOC) ? kDelkaMaxNocMin : kDelkaMaxVtMin;
}

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

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                     lv_event_cb_t cb, void* userData, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) {
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, userData);
  }
  lv_obj_t* lbl = lv_label_create(obj);
  lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

uint32_t barvaBunky(PlanAkce akce) {
  switch (akce) {
    case PLAN_AKCE_UTLUM:
      return kColOrange;
    case PLAN_AKCE_VYP:
      return kColRed;
    case PLAN_AKCE_NORMAL:
    default:
      return 0x48484Au;
  }
}

int casNaMinuty(const PlanCas* cas) {
  return static_cast<int>(cas->hodina) * 60 + static_cast<int>(cas->minuta);
}

void minutyNaCas(int total, PlanCas* cas) {
  total = ((total % (24 * 60)) + (24 * 60)) % (24 * 60);
  cas->hodina = static_cast<uint8_t>(total / 60);
  cas->minuta = static_cast<uint8_t>(total % 60);
}

int delkaObdobiMin(const PlanObdobiCas* ob) {
  const int zac = casNaMinuty(&ob->zacatek);
  const int kon = casNaMinuty(&ob->konec);
  if (kon >= zac) {
    return kon - zac;
  }
  return (24 * 60 - zac) + kon;
}

void nastavDelkuObdobi(PlanObdobiCas* ob, int delkaMin, int maxDelkaMin) {
  if (delkaMin < kDelkaMinMin) {
    delkaMin = kDelkaMinMin;
  }
  if (delkaMin > maxDelkaMin) {
    delkaMin = maxDelkaMin;
  }
  minutyNaCas(casNaMinuty(&ob->zacatek) + delkaMin, &ob->konec);
}

void posunZacatek(PlanObdobiCas* ob, int deltaMin, int maxDelkaMin) {
  const int delka = delkaObdobiMin(ob);
  minutyNaCas(casNaMinuty(&ob->zacatek) + deltaMin, &ob->zacatek);
  nastavDelkuObdobi(ob, delka, maxDelkaMin);
}

void posunDelku(PlanObdobiCas* ob, int deltaMin, int maxDelkaMin) {
  nastavDelkuObdobi(ob, delkaObdobiMin(ob) + deltaMin, maxDelkaMin);
}

void formatBunkaText(char* buf, size_t len, const PlanBunka* bunka) {
  switch (bunka->akce) {
    case PLAN_AKCE_UTLUM:
      snprintf(buf, len, "-%u st", (unsigned)bunka->utlum_stupne);
      break;
    case PLAN_AKCE_VYP:
      strncpy(buf, "Vyp", len);
      break;
    case PLAN_AKCE_NORMAL:
    default:
      strncpy(buf, "Norm", len);
      break;
  }
}

void cyklujBunku(PlanBunka* bunka) {
  switch (bunka->akce) {
    case PLAN_AKCE_NORMAL:
      bunka->akce = PLAN_AKCE_UTLUM;
      bunka->utlum_stupne = static_cast<uint8_t>(kMinUtlumStupne);
      break;
    case PLAN_AKCE_UTLUM:
      if (bunka->utlum_stupne < kMaxUtlumStupne) {
        bunka->utlum_stupne++;
      } else {
        bunka->akce = PLAN_AKCE_VYP;
        bunka->utlum_stupne = 0;
      }
      break;
    case PLAN_AKCE_VYP:
    default:
      bunka->akce = PLAN_AKCE_NORMAL;
      bunka->utlum_stupne = 0;
      break;
  }
}

void refreshBunka(uint8_t den, uint8_t obdobi) {
  lv_obj_t* btn = planObj.btn_bunky[den][obdobi];
  if (!btn) {
    return;
  }
  const PlanBunka* bunka = &g_planConfig.tabulka[den][obdobi];
  char txt[12];
  formatBunkaText(txt, sizeof(txt), bunka);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, txt);
  }
  const uint32_t col = barvaBunky(bunka->akce);
  static uint32_t s_cellBg[PLAN_POCET_DNU][PLAN_POCET_OBDOBI] = {};
  if (s_cellBg[den][obdobi] != col) {
    s_cellBg[den][obdobi] = col;
    lv_obj_set_style_bg_color(btn, lv_color_hex(col), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void refreshCasKarta(uint8_t obdobi) {
  lv_obj_t* btn = planObj.btn_cas_karta[obdobi];
  if (!btn) {
    return;
  }
  char nameLine[8];
  char odLine[16];
  char delLine[20];
  const PlanObdobiCas* ob = &g_planConfig.obdobi[obdobi];
  snprintf(nameLine, sizeof(nameLine), "%s", climatePlanObdobiNazev(obdobi));
  snprintf(odLine, sizeof(odLine), "Od %02u:%02u",
           (unsigned)ob->zacatek.hodina, (unsigned)ob->zacatek.minuta);
  snprintf(delLine, sizeof(delLine), "Do %02u:%02u",
           (unsigned)ob->konec.hodina, (unsigned)ob->konec.minuta);

  lv_obj_t* lbl0 = lv_obj_get_child(btn, 0);
  lv_obj_t* lbl1 = lv_obj_get_child(btn, 1);
  lv_obj_t* lbl2 = lv_obj_get_child(btn, 2);
  if (lbl0) {
    setLabelIfChanged(lbl0, nameLine);
  }
  if (lbl1) {
    setLabelIfChanged(lbl1, odLine);
  }
  if (lbl2) {
    setLabelIfChanged(lbl2, delLine);
  }
}

void refreshModalLabels(void) {
  if (s_modalObdobi < 0) {
    return;
  }
  const PlanObdobiCas* ob = &g_planConfig.obdobi[static_cast<uint8_t>(s_modalObdobi)];
  const int delka = delkaObdobiMin(ob);
  (void)delka;

  char title[28];
  snprintf(title, sizeof(title), "%s - čas období",
           climatePlanObdobiNazev(static_cast<uint8_t>(s_modalObdobi)));
  setLabelIfChanged(planObj.modal_title, title);

  char odLine[16];
  snprintf(odLine, sizeof(odLine), "Od  %02u:%02u",
           (unsigned)ob->zacatek.hodina, (unsigned)ob->zacatek.minuta);
  setLabelIfChanged(planObj.modal_lbl_od, odLine);

  char delLine[16];
  snprintf(delLine, sizeof(delLine), "Délka  %u:%02u",
           (unsigned)(delka / 60), (unsigned)(delka % 60));
  setLabelIfChanged(planObj.modal_lbl_delka, delLine);

  if (s_modalHint) {
    const int maxH = maxDelkaProObdobi(static_cast<uint8_t>(s_modalObdobi)) / 60;
    char hint[36];
    snprintf(hint, sizeof(hint), "krok 30 min, max %d hod", maxH);
    setLabelIfChanged(s_modalHint, hint);
  }
}

void onCellClick(lv_event_t* e) {
  const CellRef* ref = static_cast<const CellRef*>(lv_event_get_user_data(e));
  if (!ref) {
    return;
  }
  const uint16_t key =
      static_cast<uint16_t>((static_cast<uint16_t>(ref->den) << 8) | ref->obdobi);
  const uint32_t now = millis();
  if (key == s_lastCellKey && (now - s_lastCellClickMs) < 250) {
    return;
  }
  s_lastCellKey = key;
  s_lastCellClickMs = now;

  cyklujBunku(&g_planConfig.tabulka[ref->den][ref->obdobi]);
  s_planDirty = true;
  refreshBunka(ref->den, ref->obdobi);
}

void onCasKartaClick(lv_event_t* e) {
  const ObdobiRef* ref = static_cast<const ObdobiRef*>(lv_event_get_user_data(e));
  if (!ref) {
    return;
  }
  uiPlanOpenModal(ref->obdobi);
}

void onModalBgClick(lv_event_t* e) {
  (void)e;
  uiPlanCloseModal(false);
}

void onModalAction(lv_event_t* e) {
  const ModalAction* action = static_cast<const ModalAction*>(lv_event_get_user_data(e));
  if (!action || s_modalObdobi < 0) {
    return;
  }

  if (*action == ModalAction::Hotovo) {
    uiPlanCloseModal(true);
    return;
  }

  PlanObdobiCas* ob = &g_planConfig.obdobi[static_cast<uint8_t>(s_modalObdobi)];
  ob->cas_rezim = PLAN_CAS_OD_DELKA;
  s_modalEdited = true;
  s_planDirty = true;
  const int maxDelka = maxDelkaProObdobi(static_cast<uint8_t>(s_modalObdobi));

  switch (*action) {
    case ModalAction::OdMinus:
      posunZacatek(ob, -kCasStepMin, maxDelka);
      break;
    case ModalAction::OdPlus:
      posunZacatek(ob, kCasStepMin, maxDelka);
      break;
    case ModalAction::DelMinus:
      posunDelku(ob, -kCasStepMin, maxDelka);
      break;
    case ModalAction::DelPlus:
      posunDelku(ob, kCasStepMin, maxDelka);
      break;
    default:
      break;
  }

  refreshModalLabels();
  refreshCasKarta(static_cast<uint8_t>(s_modalObdobi));
}

void styleToggleBtn(lv_obj_t* btn, bool on) {
  if (!btn) {
    return;
  }
  static bool s_lastOn = false;
  static bool s_hasLast = false;
  if (s_hasLast && s_lastOn == on) {
    return;
  }
  s_hasLast = true;
  s_lastOn = on;
  lv_obj_set_style_bg_color(
      btn, lv_color_hex(on ? 0x30D158u : 0x48484Au), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, on ? "Aktivní" : "Neaktivní");
  }
}

int colX(int colW, uint8_t obdobi) {
  return kPad + kDenW + kColGap + static_cast<int>(obdobi) * (colW + kColGap);
}

lv_obj_t* makeCasKarta(lv_obj_t* parent, int x, int y, int w, int h, uint8_t obdobi) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x24242Eu), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(btn, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btn, onCasKartaClick, LV_EVENT_CLICKED, &s_obdobiRefs[obdobi]);

  lv_obj_t* lblName = lv_label_create(btn);
  lv_obj_set_style_text_font(lblName, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblName, lv_color_hex(kColOrange), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lblName, climatePlanObdobiNazev(obdobi));
  lv_obj_set_pos(lblName, 4, (h - 28) / 2);
  lv_obj_set_width(lblName, kCasNameW);
  lv_obj_set_style_text_align(lblName, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(lblName, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lblName, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* lblOd = lv_label_create(btn);
  lv_obj_set_style_text_font(lblOd, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOd, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lblOd, "Od --:--");
  lv_obj_set_pos(lblOd, kCasColX, 8);
  lv_obj_set_width(lblOd, w - kCasColX - 4);
  lv_obj_set_style_text_align(lblOd, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(lblOd, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lblOd, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* lblDel = lv_label_create(btn);
  lv_obj_set_style_text_font(lblDel, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblDel, lv_color_hex(kColMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lblDel, "Do --:--");
  lv_obj_set_pos(lblDel, kCasColX, 40);
  lv_obj_set_width(lblDel, w - kCasColX - 4);
  lv_obj_set_style_text_align(lblDel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(lblDel, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lblDel, LV_OBJ_FLAG_CLICKABLE);

  return btn;
}

void onModalPanelClick(lv_event_t* e) {
  (void)e;
}

void createModal(void) {
  planObj.modal_bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(planObj.modal_bg, kW, kH);
  lv_obj_set_pos(planObj.modal_bg, 0, 0);
  lv_obj_set_style_bg_color(planObj.modal_bg, lv_color_hex(0x000000u), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(planObj.modal_bg, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(planObj.modal_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(planObj.modal_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(planObj.modal_bg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(planObj.modal_bg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(planObj.modal_bg, onModalBgClick, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(planObj.modal_bg, LV_OBJ_FLAG_HIDDEN);

  const int mx = (kW - kModalW) / 2;
  const int my = (kH - kModalH) / 2;
  planObj.modal_panel = makePanel(planObj.modal_bg, mx, my, kModalW, kModalH);
  lv_obj_add_flag(planObj.modal_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(planObj.modal_panel, onModalPanelClick, LV_EVENT_CLICKED, nullptr);

  planObj.modal_title = lv_label_create(planObj.modal_panel);
  lv_obj_set_pos(planObj.modal_title, kPad, 12);
  lv_obj_set_width(planObj.modal_title, kModalW - 2 * kPad);
  lv_obj_set_style_text_font(planObj.modal_title, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(planObj.modal_title, lv_color_hex(kColOrange), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(planObj.modal_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(planObj.modal_title, "Čas období");

  planObj.modal_lbl_od = lv_label_create(planObj.modal_panel);
  lv_obj_set_pos(planObj.modal_lbl_od, kPad, 50);
  lv_obj_set_width(planObj.modal_lbl_od, kModalW - 2 * kPad);
  lv_obj_set_style_text_font(planObj.modal_lbl_od, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(planObj.modal_lbl_od, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(planObj.modal_lbl_od, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(planObj.modal_lbl_od, "Od  --:--");

  const int row1y = 78;
  const int cx = (kModalW - 2 * kModalBtnW - 16) / 2;
  static ModalAction s_odMinus = ModalAction::OdMinus;
  static ModalAction s_odPlus = ModalAction::OdPlus;
  planObj.modal_btn_od_minus = makeButton(
      planObj.modal_panel, cx, row1y, kModalBtnW, kModalBtnH, "-", onModalAction, &s_odMinus, 0x3A3A3Cu);
  planObj.modal_btn_od_plus = makeButton(
      planObj.modal_panel, cx + kModalBtnW + 12, row1y, kModalBtnW, kModalBtnH, "+", onModalAction, &s_odPlus,
      0x3A3A3Cu);

  planObj.modal_lbl_delka = lv_label_create(planObj.modal_panel);
  lv_obj_set_pos(planObj.modal_lbl_delka, kPad, 132);
  lv_obj_set_width(planObj.modal_lbl_delka, kModalW - 2 * kPad);
  lv_obj_set_style_text_font(planObj.modal_lbl_delka, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(planObj.modal_lbl_delka, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(planObj.modal_lbl_delka, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(planObj.modal_lbl_delka, "Délka  --:--");

  lv_obj_t* lblHint = lv_label_create(planObj.modal_panel);
  s_modalHint = lblHint;
  lv_obj_set_pos(lblHint, kPad, 156);
  lv_obj_set_width(lblHint, kModalW - 2 * kPad);
  lv_obj_set_style_text_font(lblHint, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblHint, lv_color_hex(kColMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(lblHint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lblHint, "krok 30 min, max 4 hod");

  const int row2y = 182;
  static ModalAction s_delMinus = ModalAction::DelMinus;
  static ModalAction s_delPlus = ModalAction::DelPlus;
  planObj.modal_btn_del_minus = makeButton(
      planObj.modal_panel, cx, row2y, kModalBtnW, kModalBtnH, "-", onModalAction, &s_delMinus, 0x3A3A3Cu);
  planObj.modal_btn_del_plus = makeButton(
      planObj.modal_panel, cx + kModalBtnW + 12, row2y, kModalBtnW, kModalBtnH, "+", onModalAction, &s_delPlus,
      0x3A3A3Cu);

  static ModalAction s_hotovo = ModalAction::Hotovo;
  planObj.modal_btn_hotovo = makeButton(
      planObj.modal_panel, kPad, kModalH - kModalBtnH - kPad, kModalW - 2 * kPad, kModalBtnH,
      "Hotovo", onModalAction, &s_hotovo, kColPurple);
}

}  // namespace

void uiPlanMarkDirty(void) {
  s_planDirty = true;
}

void uiPlanResetInput(void) {
  lv_indev_t* indev = lv_indev_get_next(nullptr);
  while (indev) {
    if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
      lv_indev_reset(indev, nullptr);
    }
    indev = lv_indev_get_next(indev);
  }
}

void uiPlanFlushSave(void) {
  if (!s_planDirty) {
    return;
  }
  // Bez LVGL freeze — Preferences na UI vlákně + freeze = zamrzlý touch a RGB desync
  climatePlanSave();
  s_planDirty = false;
}

void uiPlanCloseModal(bool commit) {
  const int8_t obdobi = s_modalObdobi;
  if (obdobi >= 0) {
    if (!commit && s_modalEdited) {
      g_planConfig.obdobi[static_cast<uint8_t>(obdobi)] = s_modalBackup;
      refreshCasKarta(static_cast<uint8_t>(obdobi));
    }
    if (commit) {
      uiPlanMarkDirty();
      refreshCasKarta(static_cast<uint8_t>(obdobi));
    }
  }
  s_modalObdobi = -1;
  s_modalEdited = false;
  if (planObj.modal_bg) {
    lv_obj_add_flag(planObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
  }
  uiPlanResetInput();
}

void uiPlanOpenModal(uint8_t obdobi) {
  s_modalBackup = g_planConfig.obdobi[obdobi];
  s_modalEdited = false;
  s_modalObdobi = static_cast<int8_t>(obdobi);
  if (planObj.modal_bg) {
    lv_obj_remove_flag(planObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
  }
  refreshModalLabels();
}

void uiPlanRefreshAll(void) {
  styleToggleBtn(planObj.btn_toggle, g_planConfig.aktivni);
  for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
    refreshCasKarta(o);
  }
  for (uint8_t d = 0; d < PLAN_POCET_DNU; ++d) {
    for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      refreshBunka(d, o);
    }
  }
  if (s_modalObdobi >= 0) {
    refreshModalLabels();
  }
}

void uiPlanCreate(void) {
  if (s_planCreated) {
    return;
  }
  memset(&planObj, 0, sizeof(planObj));
  s_modalObdobi = -1;

  lv_obj_t* scr = lv_obj_create(nullptr);
  planObj.screen = scr;
  lv_obj_set_size(scr, kW, kH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  planObj.btn_back = makeButton(
      scr, kMargin, 4, 120, kBtnH, "<- ZPĚT", action_akce_plan_back, nullptr, 0x48484Fu);

  planObj.lbl_title = lv_label_create(scr);
  lv_label_set_text(planObj.lbl_title, "ČASOVÝ PLÁN");
  lv_obj_set_style_text_font(planObj.lbl_title, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(planObj.lbl_title, lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_align(planObj.lbl_title, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_remove_flag(planObj.lbl_title, LV_OBJ_FLAG_CLICKABLE);

  const int toggleW = 160;
  planObj.btn_toggle = makeButton(
      scr, kW - kMargin - toggleW, 4, toggleW, kBtnH,
      "Neaktivní", action_akce_plan_toggle, nullptr, kColPurple);

  const int tableW = kW - 2 * kMargin;
  const int colW = (tableW - kDenW - PLAN_POCET_OBDOBI * kColGap) / PLAN_POCET_OBDOBI;
  const int kartaH = kTimeRowH - 4;

  lv_obj_t* timePanel = makePanel(scr, kMargin, kHeaderH, tableW, kTimeRowH);
  planObj.lbl_col_den = lv_label_create(timePanel);
  lv_obj_set_pos(planObj.lbl_col_den, kPad, (kTimeRowH - 28) / 2);
  lv_obj_set_style_text_font(planObj.lbl_col_den, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(planObj.lbl_col_den, lv_color_hex(kColMuted), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(planObj.lbl_col_den, "Čas");

  for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
    s_obdobiRefs[o] = {o};
    const int x = colX(colW, o);
    planObj.btn_cas_karta[o] = makeCasKarta(timePanel, x, 4, colW, kartaH, o);
  }

  const int tableH = kH - kTableTop - kMargin;
  lv_obj_t* tablePanel = makePanel(scr, kMargin, kTableTop, tableW, tableH);
  const int rowGap = 4;
  const int rowH =
      (tableH - 2 * kPad - (PLAN_POCET_DNU - 1) * rowGap) / PLAN_POCET_DNU;
  const int cellH = rowH - 2;
  const int innerY = kPad;

  for (uint8_t d = 0; d < PLAN_POCET_DNU; ++d) {
    const int rowY = innerY + static_cast<int>(d) * (rowH + rowGap);
    planObj.lbl_dny[d] = lv_label_create(tablePanel);
    lv_obj_set_pos(planObj.lbl_dny[d], kPad, rowY + (cellH - 28) / 2);
    lv_obj_set_width(planObj.lbl_dny[d], kDenW);
    lv_obj_set_style_text_font(planObj.lbl_dny[d], kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(planObj.lbl_dny[d], lv_color_hex(kColText), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(planObj.lbl_dny[d], climatePlanDenNazev(d));

    for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      const int x = colX(colW, o);
      s_cellRefs[d][o] = {d, o};
      char txt[12];
      formatBunkaText(txt, sizeof(txt), &g_planConfig.tabulka[d][o]);
      planObj.btn_bunky[d][o] = makeButton(
          tablePanel, x, rowY, colW, cellH, txt, onCellClick, &s_cellRefs[d][o],
          barvaBunky(g_planConfig.tabulka[d][o].akce));
    }
  }

  createModal();

  s_planCreated = true;
  uiPlanRefreshAll();
}

void uiPlanEnsureCreated(void) {
  if (!s_planCreated) {
    uiPlanCreate();
  }
}

void uiPlanTick(void) {
  (void)s_planCreated;
}

void uiPlanOnLeave(void) {
  if (s_modalObdobi >= 0) {
    uiPlanCloseModal(false);
  }
  if (planObj.modal_bg) {
    lv_obj_add_flag(planObj.modal_bg, LV_OBJ_FLAG_HIDDEN);
  }
  // Uložení až po přepnutí obrazovky (viz uiNavigateTo) — Zpět zůstane responzivní
  uiPlanResetInput();
  if (uiLvglIsFrozen()) {
    uiLvglSetFrozen(false);
  }
}

lv_obj_t* uiPlanScreen(void) {
  return planObj.screen;
}
