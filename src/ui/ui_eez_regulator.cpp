#include "ui_eez_regulator.h"

#include "board_7b.h"
#include "climate_regulator.h"
#include "ui_eez_actions.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"

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
constexpr int kBtnH = 34;
constexpr int kPad = 8;

const lv_font_t* kFont = &ui_font_font_cs_16;
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

void markDirty() { s_dirty = true; }

void onBack(lv_event_t* e) {
  (void)e;
  uiEez.akce_tlacitko = UI_AKCE_PLAN_BACK;
}

void onModeToggle(lv_event_t* e) {
  (void)e;
  uiEez.akce_tlacitko = UI_AKCE_REZIM_PREPNOUT;
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
  adjustFloat(&climateRegulatorGetConfigMutable()->kp, -0.5f, 0.0f, 40.0f);
}
void onKpP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->kp, 0.5f, 0.0f, 40.0f);
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
void onBiasM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->bias_pct, -1.0f, -25.0f, 25.0f);
}
void onBiasP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->bias_pct, 1.0f, -25.0f, 25.0f);
}
void onColdM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_out_cold_c, -1.0f, -30.0f, 0.0f);
}
void onColdP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_out_cold_c, 1.0f, -30.0f, 0.0f);
}
void onWarmM(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_out_warm_c, -1.0f, 5.0f, 25.0f);
}
void onWarmP(lv_event_t* e) {
  (void)e;
  adjustFloat(&climateRegulatorGetConfigMutable()->t_out_warm_c, 1.0f, 5.0f, 25.0f);
}

void refreshChart() {
  if (!regulatorObj.chart || !regulatorObj.ser_room) {
    return;
  }
  const int n = climateRegulatorHistoryCount();
  lv_chart_set_point_count(regulatorObj.chart, REG_HISTORY_LEN);
  for (int i = 0; i < REG_HISTORY_LEN; ++i) {
    if (i < n) {
      RegulatorHistoryPoint p{};
      climateRegulatorHistoryGet(i, &p);
      const int16_t room =
          (p.room_c <= UI_TEPLOTA_NEPLATNA + 100.0f) ? 0 : (int16_t)lroundf(p.room_c * 10.0f);
      const int16_t sp = (int16_t)lroundf(p.room_sp_c * 10.0f);
      const int16_t u = (int16_t)lroundf(p.u_pct);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_room, i, room);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_sp, i, sp);
      lv_chart_set_value_by_id(regulatorObj.chart, regulatorObj.ser_u, i, u);
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
      makeButton(scr, kMargin, 4, 120, kBtnH, "<- ZPET", onBack, 0x48484Fu);

  regulatorObj.lbl_title = makeLabel(scr, 0, 10, 0, "REGULATOR", kColText);
  lv_obj_set_style_align(regulatorObj.lbl_title, LV_ALIGN_TOP_MID,
                         LV_PART_MAIN | LV_STATE_DEFAULT);

  regulatorObj.btn_mode =
      makeButton(scr, kW - kMargin - 140, 4, 140, kBtnH, "Auto", onModeToggle, kColGreen);
  regulatorObj.btn_ekv =
      makeButton(scr, kW - kMargin - 290, 4, 130, kBtnH, "PID only", onEkvToggle, kColOrange);
  regulatorObj.btn_save =
      makeButton(scr, kW - kMargin - 430, 4, 120, kBtnH, "Ulozit", onSave, kColAccent);

  const int contentW = kW - 2 * kMargin;
  const int topY = 48;
  const int chartH = 260;
  lv_obj_t* chartPanel = makePanel(scr, kMargin, topY, contentW, chartH);

  makeLabel(chartPanel, kPad, 4, 400, "Graf: pokoj (zel) / SP (oranz) / u% (modra)",
            kColMuted);

  regulatorObj.chart = lv_chart_create(chartPanel);
  lv_obj_set_pos(regulatorObj.chart, kPad, 28);
  lv_obj_set_size(regulatorObj.chart, contentW - 2 * kPad, chartH - 40);
  lv_chart_set_type(regulatorObj.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(regulatorObj.chart, REG_HISTORY_LEN);
  lv_chart_set_range(regulatorObj.chart, LV_CHART_AXIS_PRIMARY_Y, 150, 250);  // 15.0–25.0 °C ×10
  lv_chart_set_range(regulatorObj.chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
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
  makeLabel(pidPanel, kPad, 4, colW - 2 * kPad, "PID / krivka", kColOrange);
  regulatorObj.lbl_pid =
      makeLabel(pidPanel, kPad, 26, colW - 2 * kPad, "Kp Ki Kd", kColText);
  regulatorObj.lbl_curve =
      makeLabel(pidPanel, kPad, 70, colW - 2 * kPad, "krivka", kColMuted);

  const int bw = 44;
  const int bh = 30;
  int by = bottomH - 2 * (bh + 4) - kPad;
  int bx = kPad;
  makeLabel(pidPanel, bx, by - 18, 80, "Kp", kColMuted);
  regulatorObj.btn_kp_m = makeButton(pidPanel, bx, by, bw, bh, "-", onKpM, kColPurple);
  regulatorObj.btn_kp_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onKpP, kColPurple);
  bx += 2 * (bw + 4) + 12;
  makeLabel(pidPanel, bx, by - 18, 80, "Ki", kColMuted);
  regulatorObj.btn_ki_m = makeButton(pidPanel, bx, by, bw, bh, "-", onKiM, kColPurple);
  regulatorObj.btn_ki_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onKiP, kColPurple);
  bx += 2 * (bw + 4) + 12;
  makeLabel(pidPanel, bx, by - 18, 80, "Kd", kColMuted);
  regulatorObj.btn_kd_m = makeButton(pidPanel, bx, by, bw, bh, "-", onKdM, kColPurple);
  regulatorObj.btn_kd_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onKdP, kColPurple);

  by += bh + 8;
  bx = kPad;
  makeLabel(pidPanel, bx, by - 18, 80, "bias", kColMuted);
  regulatorObj.btn_bias_m = makeButton(pidPanel, bx, by, bw, bh, "-", onBiasM, kColPurple);
  regulatorObj.btn_bias_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onBiasP, kColPurple);
  bx += 2 * (bw + 4) + 12;
  makeLabel(pidPanel, bx, by - 18, 100, "Tcold", kColMuted);
  regulatorObj.btn_cold_m = makeButton(pidPanel, bx, by, bw, bh, "-", onColdM, kColPurple);
  regulatorObj.btn_cold_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onColdP, kColPurple);
  bx += 2 * (bw + 4) + 12;
  makeLabel(pidPanel, bx, by - 18, 100, "Twarm", kColMuted);
  regulatorObj.btn_warm_m = makeButton(pidPanel, bx, by, bw, bh, "-", onWarmM, kColPurple);
  regulatorObj.btn_warm_p =
      makeButton(pidPanel, bx + bw + 4, by, bw, bh, "+", onWarmP, kColPurple);

  s_created = true;
  uiRegulatorTick();
}

void uiRegulatorEnsureCreated(void) {
  if (!s_created) {
    uiRegulatorCreate();
  }
}

void uiRegulatorOnLeave(void) {
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

  RegulatorSnapshot snap{};
  climateRegulatorGetSnapshot(&snap);
  const RegulatorConfig* cfg = climateRegulatorGetConfig();

  lv_obj_t* modeLbl = lv_obj_get_child(regulatorObj.btn_mode, 0);
  if (modeLbl) {
    setLabelIfChanged(modeLbl, snap.active ? "Auto ON" : "Vystupni T");
  }
  lv_obj_set_style_bg_color(
      regulatorObj.btn_mode,
      lv_color_hex(snap.active ? kColGreen : 0x48484Au),
      LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t* ekvLbl = lv_obj_get_child(regulatorObj.btn_ekv, 0);
  if (ekvLbl) {
    setLabelIfChanged(ekvLbl, snap.use_equitherm ? "Ekv+PID" : "PID only");
  }
  lv_obj_set_style_bg_color(
      regulatorObj.btn_ekv,
      lv_color_hex(snap.use_equitherm ? kColPurple : kColOrange),
      LV_PART_MAIN | LV_STATE_DEFAULT);

  char line[220];
  snprintf(line, sizeof(line),
           "%s\n"
           "u %.0f%% = base %.0f + trim %.0f\n"
           "T vody %u C   e %.2f C\n"
           "pokoj %.1f / SP %.1f\n"
           "venku %.1f   BLE room %s out %s%s",
           snap.use_equitherm ? "rezim: ekviterm+PID" : "rezim: PID only (bez venku)",
           (double)snap.u_pct, (double)snap.u_base_pct, (double)snap.u_trim_pct,
           (unsigned)snap.t_water_c, (double)snap.error_c,
           snap.room_ok ? (double)snap.room_c : -999.0, (double)snap.room_sp_c,
           snap.outdoor_ok ? (double)snap.outdoor_c : -999.0,
           snap.room_ok ? "OK" : "OFF", snap.outdoor_ok ? "OK" : "OFF",
           s_dirty ? " *" : "");
  setLabelIfChanged(regulatorObj.lbl_live, line);

  snprintf(line, sizeof(line), "Kp %.1f  Ki %.2f  Kd %.1f  bias %.1f%%",
           (double)cfg->kp, (double)cfg->ki, (double)cfg->kd, (double)cfg->bias_pct);
  setLabelIfChanged(regulatorObj.lbl_pid, line);

  if (snap.use_equitherm) {
    snprintf(line, sizeof(line),
             "krivka: %.0f C -> %.0f%%   %.0f C -> %.0f%%",
             (double)cfg->t_out_cold_c, (double)cfg->u_cold_pct,
             (double)cfg->t_out_warm_c, (double)cfg->u_warm_pct);
  } else {
    snprintf(line, sizeof(line), "krivka vypnuta — base 50%%, trim ±50%%");
  }
  setLabelIfChanged(regulatorObj.lbl_curve, line);

  refreshChart();
}
