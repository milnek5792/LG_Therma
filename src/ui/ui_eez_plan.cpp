#include "ui_eez_plan.h"

#include "board_7b.h"
#include "climate_plan.h"
#include "ui_eez_actions.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

plan_objects_t planObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColPurple = 0x5856D6u;
constexpr uint32_t kColRed = 0xFF453Au;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 10;
constexpr int kHeaderH = 44;
constexpr int kBtnH = 34;
constexpr int kPad = 8;
constexpr int kDenW = 52;
constexpr int kColGap = 4;
constexpr int kMaxUtlumStupne = 5;
constexpr int kTimeRowH = 56;
constexpr int kTableTop = kHeaderH + kTimeRowH + 6;

const lv_font_t* kFont = &ui_font_font_cs_16;

struct CellRef {
  uint8_t den;
  uint8_t obdobi;
};

struct TimeRef {
  uint8_t obdobi;
  bool zacatek;
};

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
                     const char* text, lv_event_cb_t cb, void* userData, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, userData);
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

void formatCasRozsah(char* buf, size_t len, const PlanObdobiCas* obdobi) {
  snprintf(buf, len, "%02u:%02u-%02u:%02u",
           (unsigned)obdobi->zacatek.hodina, (unsigned)obdobi->zacatek.minuta,
           (unsigned)obdobi->konec.hodina, (unsigned)obdobi->konec.minuta);
}

void posunCas(PlanCas* cas, int deltaMin) {
  int total = static_cast<int>(cas->hodina) * 60 + static_cast<int>(cas->minuta) + deltaMin;
  total = ((total % (24 * 60)) + (24 * 60)) % (24 * 60);
  cas->hodina = static_cast<uint8_t>(total / 60);
  cas->minuta = static_cast<uint8_t>(total % 60);
}

void cyklujBunku(PlanBunka* bunka) {
  switch (bunka->akce) {
    case PLAN_AKCE_NORMAL:
      bunka->akce = PLAN_AKCE_UTLUM;
      bunka->utlum_stupne = 3;
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
  lv_obj_set_style_bg_color(btn, lv_color_hex(barvaBunky(bunka->akce)),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
}

void onCellClick(lv_event_t* e) {
  const CellRef* ref = static_cast<const CellRef*>(lv_event_get_user_data(e));
  if (!ref) {
    return;
  }
  cyklujBunku(&g_planConfig.tabulka[ref->den][ref->obdobi]);
  climatePlanSave();
  refreshBunka(ref->den, ref->obdobi);
}

void onTimeClick(lv_event_t* e) {
  const TimeRef* ref = static_cast<const TimeRef*>(lv_event_get_user_data(e));
  if (!ref) {
    return;
  }
  PlanObdobiCas* ob = &g_planConfig.obdobi[ref->obdobi];
  if (ref->zacatek) {
    posunCas(&ob->zacatek, 30);
  } else {
    posunCas(&ob->konec, 30);
  }
  climatePlanSave();
  uiPlanRefreshAll();
}

static CellRef s_cellRefs[PLAN_POCET_DNU][PLAN_POCET_OBDOBI];
static TimeRef s_timeRefsZac[PLAN_POCET_OBDOBI];
static TimeRef s_timeRefsKon[PLAN_POCET_OBDOBI];

void styleToggleBtn(lv_obj_t* btn, bool on) {
  if (!btn) {
    return;
  }
  lv_obj_set_style_bg_color(
      btn, lv_color_hex(on ? kColGreen : 0x48484Au), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, on ? "Aktivni" : "Neaktivni");
  }
}

}  // namespace

void uiPlanRefreshAll(void) {
  for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
    char casTxt[20];
    formatCasRozsah(casTxt, sizeof(casTxt), &g_planConfig.obdobi[o]);
    if (planObj.lbl_casy[o]) {
      setLabelIfChanged(planObj.lbl_casy[o], casTxt);
    }
  }
  for (uint8_t d = 0; d < PLAN_POCET_DNU; ++d) {
    for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      refreshBunka(d, o);
    }
  }
  styleToggleBtn(planObj.btn_toggle, g_planConfig.aktivni);
}

void uiPlanCreate(void) {
  memset(&planObj, 0, sizeof(planObj));

  lv_obj_t* scr = lv_obj_create(nullptr);
  planObj.screen = scr;
  lv_obj_set_size(scr, kW, kH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  planObj.btn_back = makeButton(
      scr, kMargin, 4, 120, kBtnH, "<- ZPET", action_akce_plan_back, nullptr, 0x48484Fu);
  planObj.lbl_title = makeLabel(scr, 0, 10, 0, "CASOVY PLAN", kColText);
  lv_obj_set_style_align(planObj.lbl_title, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);

  const int toggleW = 120;
  planObj.btn_toggle = makeButton(
      scr, kW - kMargin - toggleW, 4, toggleW, kBtnH,
      "Neaktivni", action_akce_plan_toggle, nullptr, kColPurple);

  const int tableW = kW - 2 * kMargin;
  const int colW = (tableW - kDenW - PLAN_POCET_OBDOBI * kColGap) / PLAN_POCET_OBDOBI;

  lv_obj_t* timePanel = makePanel(scr, kMargin, kHeaderH, tableW, kTimeRowH);
  planObj.lbl_col_den = makeLabel(timePanel, kPad, 4, kDenW, "Den", kColMuted);

  for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
    const int x = kPad + kDenW + kColGap + static_cast<int>(o) * (colW + kColGap);
    planObj.lbl_col_obdobi[o] =
        makeLabel(timePanel, x, 4, colW, climatePlanObdobiNazev(o), kColOrange);
    lv_obj_set_style_text_align(planObj.lbl_col_obdobi[o], LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    char casTxt[20];
    formatCasRozsah(casTxt, sizeof(casTxt), &g_planConfig.obdobi[o]);
    planObj.lbl_casy[o] = makeLabel(timePanel, x, 22, colW, casTxt, kColText);
    lv_obj_set_style_text_align(planObj.lbl_casy[o], LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    s_timeRefsZac[o] = {o, true};
    s_timeRefsKon[o] = {o, false};
    const int halfW = (colW - 4) / 2;
    planObj.btn_cas_zac[o] = makeButton(
        timePanel, x, 38, halfW, 18, "<", onTimeClick, &s_timeRefsZac[o], 0x3A3A3Cu);
    planObj.btn_cas_kon[o] = makeButton(
        timePanel, x + halfW + 4, 38, halfW, 18, ">", onTimeClick, &s_timeRefsKon[o], 0x3A3A3Cu);
  }

  lv_obj_t* tablePanel = makePanel(scr, kMargin, kTableTop, tableW, kH - kTableTop - kMargin);
  const int innerY = kPad;

  for (uint8_t d = 0; d < PLAN_POCET_DNU; ++d) {
    const int rowY = innerY + static_cast<int>(d) * (kRowH + 2);
    planObj.lbl_dny[d] = makeLabel(tablePanel, kPad, rowY + 18, kDenW,
                                   climatePlanDenNazev(d), kColText);

    for (uint8_t o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      const int x = kPad + kDenW + kColGap + static_cast<int>(o) * (colW + kColGap);
      s_cellRefs[d][o] = {d, o};
      char txt[12];
      formatBunkaText(txt, sizeof(txt), &g_planConfig.tabulka[d][o]);
      planObj.btn_bunky[d][o] = makeButton(
          tablePanel, x, rowY, colW, kRowH - 4, txt, onCellClick, &s_cellRefs[d][o],
          barvaBunky(g_planConfig.tabulka[d][o].akce));
    }
  }

  uiPlanRefreshAll();
}

void uiPlanTick(void) {
  styleToggleBtn(planObj.btn_toggle, g_planConfig.aktivni);
}

lv_obj_t* uiPlanScreen(void) {
  return planObj.screen;
}
