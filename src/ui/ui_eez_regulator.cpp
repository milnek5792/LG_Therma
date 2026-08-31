#include "ui_eez_regulator.h"

#include "lg_board.h"
#include "climate_regulator.h"
#include "src/ui_eez_actions.h"
#include "src/ui_eez_fonts.h"
#include "src/ui_eez_model.h"
#include "app_cmd.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

regulator_objects_t regulatorObj;

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
constexpr uint32_t kColCyan = 0x64D2FFu;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kBtnH = 40;
constexpr int kPad = 8;

const lv_font_t* kFont = &ui_font_font_cs_24;
const lv_font_t* kFontTitle = &ui_font_font_cs_24;
bool s_created = false;
bool s_dirty = false;

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

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, int maxW, const char* text,
                    uint32_t color) {
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

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                     lv_event_cb_t cb, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) {
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
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

void markDirty() {
  s_dirty = true;
  climateRegulatorRequestSave();
}

void onBack(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_PLAN_BACK);
}

void onModeToggle(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_REZIM_PREPNOUT);
}

void onSave(lv_event_t* e) {
  (void)e;
  climateRegulatorSave();
  s_dirty = false;
}

void onEkvToggle(lv_event_t* e) {
  (void)e;
  climateRegulatorSetUseEquitherm(!climateRegulatorUseEquitherm());
  markDirty();
}

void adjustFloat(float* v, float delta, float lo, float hi) {
  if (!v) {
    return;
  }
  *v = *v + delta;
  if (*v < lo) {
    *v = lo;
  }
  if (*v > hi) {
    *v = hi;
  }
  markDirty();
}

void onKpM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->kp, -0.5f, 0.0f, 20.0f);
}
void onKpP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->kp, 0.5f, 0.0f, 20.0f);
}
void onKiM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->ki, -0.05f, 0.0f, 5.0f);
}
void onKiP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->ki, 0.05f, 0.0f, 5.0f);
}
void onKdM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->kd, -0.5f, 0.0f, 20.0f);
}
void onKdP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->kd, 0.5f, 0.0f, 20.0f);
}
void onOffsetM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->offset_c, -0.5f,
              REG_EQ_OFFSET_MIN_C, REG_EQ_OFFSET_MAX_C);
}
void onOffsetP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->offset_c, 0.5f,
              REG_EQ_OFFSET_MIN_C, REG_EQ_OFFSET_MAX_C);
}
void onColdM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_water_cold_c, -0.5f,
              REG_T_WATER_MIN_C, REG_T_WATER_MAX_C);
}
void onColdP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_water_cold_c, 0.5f,
              REG_T_WATER_MIN_C, REG_T_WATER_MAX_C);
}
void onWarmM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_water_warm_c, -0.5f,
              REG_T_WATER_MIN_C, REG_T_WATER_MAX_C);
}
void onWarmP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_water_warm_c, 0.5f,
              REG_T_WATER_MIN_C, REG_T_WATER_MAX_C);
}

void refreshChart() {
  if (!regulatorObj.chart || !regulatorObj.ser_room) {
    return;
  }
  const int n = climateRegulatorHistoryCount();
  static int s_lastPointCount = -1;
  if (s_lastPointCount != REG_HISTORY_LEN) {
    lv_chart_set_point_count(regulatorObj.chart, REG_HISTORY_LEN);
    s_lastPointCount = REG_HISTORY_LEN;
  }
  for (int i = 0; i < REG_HISTORY_LEN; ++i) {
    if (i < n) {
      RegulatorHistoryPoint p{};
      climateRegulatorHistoryGet(i, &p);
      const int16_t room =
          (p.room_c <= UI_TEPLOTA_NEPLATNA + 100.0f) ? 0 : (int16_t)lroundf(p.room_c * 10.0f);
      const int16_t sp = (int16_t)lroundf(p.room_sp_c * 10.0f);
      const int16_t tWater = (int16_t)lroundf(p.t_water_c * 10.0f);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_room, i, room);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_sp, i, sp);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_u, i, tWater);
    } else {
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_room, i,
                               LV_CHART_POINT_NONE);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_sp, i,
                               LV_CHART_POINT_NONE);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_u, i,
                               LV_CHART_POINT_NONE);
    }
  }
  lv_obj_invalidate(regulatorObj.chart);
}

bool chartNeedsRefresh(void) {
  static uint32_t s_lastGen = 0;
  const uint32_t gen = climateRegulatorHistoryGen();
  if (gen != s_lastGen) {
    s_lastGen = gen;
    return true;
  }
  return false;
}

}  // namespace

void uiRegulatorCreate(void) {
  if (s_created) {
    return;
  }
  memset(&regulatorObj, 0, sizeof(regulatorObj));

  lv_obj_t* scr = lv_obj_create(nullptr);
  regulatorObj.screen = scr;
  lv_obj_set_size(scr, kW, kH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  regulatorObj.btn_back =
      makeButton(scr, kMargin, 4, 120, kBtnH, "<- ZPĚT", onBack, 0x48484Fu);

  regulatorObj.lbl_title = makeLabel(scr, 0, 10, 0, "REGULÁTOR", kColText);
  lv_obj_set_style_text_font(regulatorObj.lbl_title, kFontTitle, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_align(regulatorObj.lbl_title, LV_ALIGN_TOP_MID,
                         LV_PART_MAIN | LV_STATE_DEFAULT);

  regulatorObj.btn_mode =
      makeButton(scr, kW - kMargin - 150, 4, 150, kBtnH, "Auto", onModeToggle, kColGreen);
  regulatorObj.btn_ekv =
      makeButton(scr, kW - kMargin - 310, 4, 140, kBtnH, "PID only", onEkvToggle, kColOrange);
  regulatorObj.btn_save =
      makeButton(scr, kW - kMargin - 460, 4, 130, kBtnH, "Uložit", onSave, kColAccent);

  const int contentW = kW - 2 * kMargin;
  const int topY = 52;
  const int chartH = 260;
  lv_obj_t* chartPanel = makePanel(scr, kMargin, topY, contentW, chartH);

  makeLabel(chartPanel, kPad, 4, 400, "Graf: pokoj (zel.) / SP (oranž.) / SP vody (modře)",
            kColMuted);

  regulatorObj.chart = lv_chart_create(chartPanel);
  lv_obj_set_pos(regulatorObj.chart, kPad, 28);
  lv_obj_set_size(regulatorObj.chart, contentW - 2 * kPad, chartH - 40);
  lv_chart_set_type(regulatorObj.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(regulatorObj.chart, REG_HISTORY_LEN);
  lv_chart_set_range(regulatorObj.chart, LV_CHART_AXIS_PRIMARY_Y, 150, 250);  // 15.0–25.0 °C ×10
  lv_chart_set_range(regulatorObj.chart, LV_CHART_AXIS_SECONDARY_Y, 200, 450);  // 20–45 °C ×10
  lv_obj_set_style_bg_color(regulatorObj.chart, lv_color_hex(0x0E0E10u),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(regulatorObj.chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  regulatorObj.ser_room =
      lv_chart_add_series(regulatorObj.chart, lv_color_hex(kColGreen),
                          LV_CHART_AXIS_PRIMARY_Y);
  regulatorObj.ser_sp =
      lv_chart_add_series(regulatorObj.chart, lv_color_hex(kColOrange),
                          LV_CHART_AXIS_PRIMARY_Y);
  regulatorObj.ser_u =
      lv_chart_add_series(regulatorObj.chart, lv_color_hex(kColCyan),
                          LV_CHART_AXIS_SECONDARY_Y);

  const int bottomY = topY + chartH + kGap;
  const int bottomH = kH - bottomY - kMargin;
  const int colW = (contentW - kGap) / 2;

  lv_obj_t* livePanel = makePanel(scr, kMargin, bottomY, colW, bottomH);
  makeLabel(livePanel, kPad, 4, colW - 2 * kPad, "Stav", kColOrange);
  regulatorObj.lbl_live =
      makeLabel(livePanel, kPad, 28, colW - 2 * kPad, "...", kColText);
  lv_obj_set_height(regulatorObj.lbl_live, bottomH - 40);
  lv_label_set_long_mode(regulatorObj.lbl_live, LV_LABEL_LONG_WRAP);

  lv_obj_t* pidPanel = makePanel(scr, kMargin + colW + kGap, bottomY, colW, bottomH);
  makeLabel(pidPanel, kPad, 4, colW - 2 * kPad, "PID / křivka", kColOrange);
  regulatorObj.lbl_pid =
      makeLabel(pidPanel, kPad, 26, colW - 2 * kPad, "Kp Ki Kd", kColText);
  regulatorObj.lbl_curve =
      makeLabel(pidPanel, kPad, 52, colW - 2 * kPad, "ekviterm", kColMuted);

  const int colInner = colW - 2 * kPad;
  const int groupGap = 10;
  const int groupW = (colInner - 2 * groupGap) / 3;
  const int bw = (groupW - 10) / 2;
  const int bh = 66;
  const int btnGap = 10;
  const int lblAbove = 36;
  const int rowGap = 32;

  const int paramsBlockTop = 92;
  const int byTop = paramsBlockTop + lblAbove;
  const int byBottom = byTop + bh + rowGap + lblAbove;

  auto placeParamRow = [&](int by, const char* n0, lv_event_cb_t m0, lv_event_cb_t p0,
                           const char* n1, lv_event_cb_t m1, lv_event_cb_t p1,
                           const char* n2, lv_event_cb_t m2, lv_event_cb_t p2,
                           lv_obj_t** out_m0, lv_obj_t** out_p0, lv_obj_t** out_m1,
                           lv_obj_t** out_p1, lv_obj_t** out_m2, lv_obj_t** out_p2) {
    for (int i = 0; i < 3; ++i) {
      const int gx = kPad + i * (groupW + groupGap);
      const char* name = (i == 0) ? n0 : (i == 1) ? n1 : n2;
      lv_event_cb_t cbm = (i == 0) ? m0 : (i == 1) ? m1 : m2;
      lv_event_cb_t cbp = (i == 0) ? p0 : (i == 1) ? p1 : p2;
      lv_obj_t** om = (i == 0) ? out_m0 : (i == 1) ? out_m1 : out_m2;
      lv_obj_t** op = (i == 0) ? out_p0 : (i == 1) ? out_p1 : out_p2;
      makeLabel(pidPanel, gx, by - lblAbove, groupW, name, kColMuted);
      *om = makeButton(pidPanel, gx, by, bw, bh, "-", cbm, kColPurple);
      *op = makeButton(pidPanel, gx + bw + btnGap, by, bw, bh, "+", cbp, kColPurple);
    }
  };

  placeParamRow(byTop, "Kp", onKpM, onKpP, "Ki", onKiM, onKiP, "Kd", onKdM, onKdP,
                &regulatorObj.btn_kp_m, &regulatorObj.btn_kp_p, &regulatorObj.btn_ki_m,
                &regulatorObj.btn_ki_p, &regulatorObj.btn_kd_m, &regulatorObj.btn_kd_p);
  placeParamRow(byBottom, "offset", onOffsetM, onOffsetP, "voda -15", onColdM, onColdP,
                "voda +15", onWarmM, onWarmP, &regulatorObj.btn_bias_m,
                &regulatorObj.btn_bias_p, &regulatorObj.btn_cold_m,
                &regulatorObj.btn_cold_p, &regulatorObj.btn_warm_m,
                &regulatorObj.btn_warm_p);

  const int valY = byBottom + bh + 8;
  regulatorObj.lbl_val_bias =
      makeLabel(pidPanel, kPad, valY, groupW, "offset -", kColAccent);
  regulatorObj.lbl_val_cold =
      makeLabel(pidPanel, kPad + groupW + groupGap, valY, groupW, "voda -15 -",
                kColAccent);
  regulatorObj.lbl_val_warm =
      makeLabel(pidPanel, kPad + 2 * (groupW + groupGap), valY, groupW,
                "voda +15 -", kColAccent);
  lv_obj_set_style_text_align(regulatorObj.lbl_val_bias, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(regulatorObj.lbl_val_cold, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(regulatorObj.lbl_val_warm, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  s_created = true;
  uiRegulatorTick();
}

void uiRegulatorEnsureCreated(void) {
  if (!s_created) {
    uiRegulatorCreate();
  }
}

void uiRegulatorOnLeave(void) {}

void uiRegulatorFlushSave(void) {
  if (s_dirty) {
    climateRegulatorSave();
    s_dirty = false;
  }
}

lv_obj_t* uiRegulatorScreen(void) {
  return regulatorObj.screen;
}

void uiRegulatorTick(void) {
  if (!s_created || !regulatorObj.screen) {
    return;
  }

  static uint32_t s_lastStatusMs = 0;
  static float s_lastKp = -1.0f;
  static float s_lastKi = -1.0f;
  static float s_lastKd = -1.0f;
  static float s_lastOffset = -999.0f;
  static float s_lastCold = -999.0f;
  static float s_lastWarm = -999.0f;
  static bool s_lastDirty = false;
  const uint32_t now = millis();
  const RegulatorConfig* cfg = climateRegulatorGetConfig();
  const bool paramsChanged =
      (cfg->kp != s_lastKp) || (cfg->ki != s_lastKi) || (cfg->kd != s_lastKd) ||
      (cfg->offset_c != s_lastOffset) || (cfg->t_water_cold_c != s_lastCold) ||
      (cfg->t_water_warm_c != s_lastWarm) || (s_dirty != s_lastDirty);
  if (!paramsChanged && s_lastStatusMs != 0 && (now - s_lastStatusMs) < 1000u) {
    if (chartNeedsRefresh()) {
      refreshChart();
    }
    return;
  }
  s_lastStatusMs = now;
  s_lastKp = cfg->kp;
  s_lastKi = cfg->ki;
  s_lastKd = cfg->kd;
  s_lastOffset = cfg->offset_c;
  s_lastCold = cfg->t_water_cold_c;
  s_lastWarm = cfg->t_water_warm_c;
  s_lastDirty = s_dirty;

  RegulatorSnapshot snap{};
  climateRegulatorGetSnapshot(&snap);

  lv_obj_t* modeLbl = lv_obj_get_child(regulatorObj.btn_mode, 0);
  if (modeLbl) {
    setLabelIfChanged(modeLbl, snap.active ? "Auto ON" : "Výstupní T");
  }
  static bool s_lastActive = false;
  static bool s_haveActive = false;
  if (!s_haveActive || s_lastActive != snap.active) {
    s_haveActive = true;
    s_lastActive = snap.active;
    lv_obj_set_style_bg_color(
        regulatorObj.btn_mode,
        lv_color_hex(snap.active ? kColGreen : 0x48484Au),
        LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  lv_obj_t* ekvLbl = lv_obj_get_child(regulatorObj.btn_ekv, 0);
  if (ekvLbl) {
    setLabelIfChanged(ekvLbl, snap.use_equitherm ? "EKV ON" : "fix 32.5");
  }
  static bool s_lastEkv = false;
  static bool s_haveEkv = false;
  if (!s_haveEkv || s_lastEkv != snap.use_equitherm) {
    s_haveEkv = true;
    s_lastEkv = snap.use_equitherm;
    lv_obj_set_style_bg_color(
        regulatorObj.btn_ekv,
        lv_color_hex(snap.use_equitherm ? kColAccent : 0x48484Au),
        LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  const float roomUi = uiEez.teplota_vnitrni;
  const bool roomUiOk = roomUi > (UI_TEPLOTA_NEPLATNA + 100.0f);
  const float roomSp = snap.room_sp_c;
  const float errLive = roomUiOk ? (roomSp - roomUi) : snap.error_c;
  const float outUi = uiEez.teplota_venkovni;
  const bool outUiOk = outUi > (UI_TEPLOTA_NEPLATNA + 100.0f);

  const char* modeTxt = "Výstupní T (pauza)";
  if (snap.active) {
    modeTxt = snap.eco_mode ? "Auto - Eco (kompresor vyp.)" : "Auto - běžná regulace";
  }

  char line[260];
  snprintf(line, sizeof(line),
           "%s\n"
           "základ %.1f + korekce %.1f = %.1f  LIN %u °C\n"
           "P %.1f  I %.1f °C   e %.2f °C\n"
           "pokoj %.1f / SP %.1f   venku %.1f\n"
           "senzor %s%s",
           modeTxt, (double)snap.eq_base_c, (double)snap.pid_corr_c,
           (double)snap.t_water_sp_c, (unsigned)snap.t_water_c,
           (double)snap.p_term_c, (double)snap.i_term_c, (double)errLive,
           roomUiOk ? (double)roomUi : -999.0, (double)roomSp,
           outUiOk ? (double)outUi : -999.0,
           roomUiOk ? "OK" : "OFF", s_dirty ? " *" : "");
  setLabelIfChanged(regulatorObj.lbl_live, line);

  snprintf(line, sizeof(line), "Kp %.1f  Ki %.2f  Kd %.1f  (korekce °C)",
           (double)cfg->kp, (double)cfg->ki, (double)cfg->kd);
  setLabelIfChanged(regulatorObj.lbl_pid, line);

  snprintf(line, sizeof(line), "ekv: voda -15..+15 + offset  kor -5..+3  Eco +0.3");
  setLabelIfChanged(regulatorObj.lbl_curve, line);

  snprintf(line, sizeof(line), "%+.1f C", (double)cfg->offset_c);
  setLabelIfChanged(regulatorObj.lbl_val_bias, line);

  snprintf(line, sizeof(line), "%.1f C", (double)cfg->t_water_cold_c);
  setLabelIfChanged(regulatorObj.lbl_val_cold, line);

  snprintf(line, sizeof(line), "%.1f C", (double)cfg->t_water_warm_c);
  setLabelIfChanged(regulatorObj.lbl_val_warm, line);

  if (chartNeedsRefresh()) {
    refreshChart();
  }
}
