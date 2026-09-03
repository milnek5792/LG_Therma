#include "ui_eez_energy.h"

#include "lg_board.h"
#include "climate_energy.h"
#include "climate_plan.h"
#include "app_cmd.h"
#include "src/ui_eez_actions.h"
#include "src/ui_eez_fonts.h"
#include "src/ui_eez_model.h"
#include "src/ui_eez_nav.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

energy_objects_t energyObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColPurple = 0x5856D6u;
constexpr uint32_t kColUtlum = 0xC47A12u;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kBtnH = 44;
constexpr int kPad = 8;
constexpr int kYAxisW = 52;
constexpr int kHourH = 26;
constexpr int kUtlumStripH = 10;
constexpr int kChartPoints = 288;  // 5 min
constexpr int kYTicks = 5;

const lv_font_t* kFont = &ui_font_font_cs_24;
const lv_font_t* kFontCount = &lv_font_montserrat_32;
bool s_created = false;
int s_dayOffset = 0;
uint32_t s_lastGen = 0;

lv_obj_t* s_lblHour[24] = {};
lv_obj_t* s_lblYTick[kYTicks] = {};
lv_obj_t* s_lblMonthVal[ENERGY_SEASON_MONTHS] = {};
lv_obj_t* s_lblMonthName[ENERGY_SEASON_MONTHS] = {};
lv_obj_t* s_utlumStrip = nullptr;
lv_obj_t* s_powerPanel = nullptr;
lv_obj_t* s_monthPanel = nullptr;

const char* kMonthNames[ENERGY_SEASON_MONTHS] = {
    "Zář", "Říj", "Lis", "Pro", "Led", "Úno", "Bře", "Dub", "Kvě"};

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

void setTextColor(lv_obj_t* obj, uint32_t color) {
  if (!obj) {
    return;
  }
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
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
  lv_obj_set_style_text_color(obj, lv_color_hex(color),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
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
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE |
                                                    LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  if (cb) {
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
  }
  lv_obj_t* lbl = lv_label_create(obj);
  lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

void onBack(lv_event_t* e) {
  (void)e;
  appCmdEnqueueHmi(UI_AKCE_PLAN_BACK);
}

void onDayPrev(lv_event_t* e) {
  (void)e;
  if (s_dayOffset < ENERGY_WEEK_DAYS - 1) {
    s_dayOffset++;
    s_lastGen = 0;
  }
}

void onDayNext(lv_event_t* e) {
  (void)e;
  if (s_dayOffset > 0) {
    s_dayOffset--;
    s_lastGen = 0;
  }
}

int weekdayFromYmd(int ymd) {
  if (ymd <= 0) {
    time_t now = time(nullptr);
    struct tm t{};
    localtime_r(&now, &t);
    return (t.tm_wday + 6) % 7;
  }
  struct tm t{};
  t.tm_year = (ymd / 10000) - 1900;
  t.tm_mon = ((ymd / 100) % 100) - 1;
  t.tm_mday = ymd % 100;
  t.tm_hour = 12;
  time_t tt = mktime(&t);
  struct tm out{};
  localtime_r(&tt, &out);
  return (out.tm_wday + 6) % 7;
}

uint16_t planMinutes(const PlanCas* c) {
  return (uint16_t)(c->hodina * 60 + c->minuta);
}

bool minuteInUtlum(int weekday, int minuteOfDay) {
  const PlanTydenConfig* cfg = climatePlanGetConfig();
  if (!cfg || !cfg->aktivni || weekday < 0 || weekday >= PLAN_POCET_DNU) {
    return false;
  }
  if (minuteOfDay < 0) {
    minuteOfDay += ENERGY_MINUTES_PER_DAY;
  }
  minuteOfDay %= ENERGY_MINUTES_PER_DAY;
  for (int ob = 0; ob < PLAN_POCET_OBDOBI; ++ob) {
    const PlanBunka* cell = &cfg->tabulka[weekday][ob];
    if (cell->akce != PLAN_AKCE_UTLUM) {
      continue;
    }
    const PlanObdobiCas* od = &cfg->obdobi[ob];
    const int a = (int)planMinutes(&od->zacatek);
    const int b = (int)planMinutes(&od->konec);
    if (a <= b) {
      if (minuteOfDay >= a && minuteOfDay < b) {
        return true;
      }
    } else {
      if (minuteOfDay >= a || minuteOfDay < b) {
        return true;
      }
    }
  }
  return false;
}

void refreshUtlumAxis(int weekday) {
  if (!s_utlumStrip) {
    return;
  }
  lv_obj_clean(s_utlumStrip);
  const int stripW = (int)lv_obj_get_width(s_utlumStrip);
  const int stripH = (int)lv_obj_get_height(s_utlumStrip);
  if (stripW <= 0 || stripH <= 0) {
    return;
  }

  auto addMark = [&](int fromMin, int toMin) {
    if (toMin <= fromMin) {
      return;
    }
    int x = (int)((int64_t)fromMin * stripW / ENERGY_MINUTES_PER_DAY);
    int w = (int)((int64_t)(toMin - fromMin) * stripW / ENERGY_MINUTES_PER_DAY);
    if (w < 2) {
      w = 2;
    }
    lv_obj_t* m = lv_obj_create(s_utlumStrip);
    lv_obj_set_pos(m, x, 0);
    lv_obj_set_size(m, w, stripH);
    lv_obj_set_style_bg_color(m, lv_color_hex(kColUtlum),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(m, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(m, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(m, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(m, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
  };

  const PlanTydenConfig* cfg = climatePlanGetConfig();
  if (!cfg || !cfg->aktivni || weekday < 0 || weekday >= PLAN_POCET_DNU) {
    for (int h = 0; h < 24; ++h) {
      setTextColor(s_lblHour[h], kColMuted);
    }
    return;
  }

  for (int ob = 0; ob < PLAN_POCET_OBDOBI; ++ob) {
    if (cfg->tabulka[weekday][ob].akce != PLAN_AKCE_UTLUM) {
      continue;
    }
    const PlanObdobiCas* od = &cfg->obdobi[ob];
    const int a = (int)planMinutes(&od->zacatek);
    const int b = (int)planMinutes(&od->konec);
    if (a <= b) {
      addMark(a, b);
    } else {
      addMark(a, ENERGY_MINUTES_PER_DAY);
      addMark(0, b);
    }
  }

  for (int h = 0; h < 24; ++h) {
    // Hodina 1 = 00:00–01:00 … hodina 24 = 23:00–24:00
    const bool utl = minuteInUtlum(weekday, h * 60 + 30);
    setTextColor(s_lblHour[h], utl ? kColOrange : kColMuted);
  }
}

void refreshBands(int weekday) {
  if (!energyObj.band_layer) {
    return;
  }
  lv_obj_clean(energyObj.band_layer);
  const PlanTydenConfig* cfg = climatePlanGetConfig();
  if (!cfg || !cfg->aktivni || weekday < 0 || weekday >= PLAN_POCET_DNU) {
    return;
  }

  const int layerW = (int)lv_obj_get_width(energyObj.band_layer);
  const int layerH = (int)lv_obj_get_height(energyObj.band_layer);
  if (layerW <= 0 || layerH <= 0) {
    return;
  }

  auto addBand = [&](uint16_t fromMin, uint16_t toMin) {
    if (toMin <= fromMin) {
      return;
    }
    const int x = (int)((int64_t)fromMin * layerW / ENERGY_MINUTES_PER_DAY);
    int w = (int)((int64_t)(toMin - fromMin) * layerW / ENERGY_MINUTES_PER_DAY);
    if (w < 1) {
      w = 1;
    }
    lv_obj_t* band = lv_obj_create(energyObj.band_layer);
    lv_obj_set_pos(band, x, 0);
    lv_obj_set_size(band, w, layerH);
    lv_obj_set_style_bg_color(band, lv_color_hex(kColUtlum),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(band, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(band, LV_OBJ_FLAG_CLICKABLE);
  };

  for (int ob = 0; ob < PLAN_POCET_OBDOBI; ++ob) {
    const PlanBunka* cell = &cfg->tabulka[weekday][ob];
    if (cell->akce != PLAN_AKCE_UTLUM) {
      continue;
    }
    const PlanObdobiCas* od = &cfg->obdobi[ob];
    const uint16_t a = planMinutes(&od->zacatek);
    const uint16_t b = planMinutes(&od->konec);
    if (a <= b) {
      addBand(a, b);
    } else {
      addBand(a, ENERGY_MINUTES_PER_DAY);
      addBand(0, b);
    }
  }
}

void refreshYTicks(int32_t maxTenthKw) {
  if (maxTenthKw < 10) {
    maxTenthKw = 10;
  }
  const int chartH = energyObj.chart_power
                         ? (int)lv_obj_get_height(energyObj.chart_power)
                         : 0;
  const int chartY = energyObj.chart_power
                         ? (int)lv_obj_get_y(energyObj.chart_power)
                         : 0;
  for (int i = 0; i < kYTicks; ++i) {
    if (!s_lblYTick[i]) {
      continue;
    }
    const int32_t v = maxTenthKw * (kYTicks - 1 - i) / (kYTicks - 1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", (double)v / 10.0);
    setLabelIfChanged(s_lblYTick[i], buf);
    const int y = chartY + (chartH - 22) * i / (kYTicks - 1);
    lv_obj_set_y(s_lblYTick[i], y);
  }
}

void refreshPowerChart() {
  const uint16_t* samples = nullptr;
  float dayKwh = 0.0f;
  int ymd = 0;
  if (!climateEnergyDayPowerGet(s_dayOffset, &samples, &dayKwh, &ymd) ||
      !samples || !energyObj.ser_power) {
    return;
  }

  uint16_t maxW = 0;
  for (int i = 0; i < ENERGY_MINUTES_PER_DAY; ++i) {
    if (samples[i] > maxW) {
      maxW = samples[i];
    }
  }
  // Y v desetinách kW, zaokrouhlení nahoru na 0,5 kW, min. 1,0 kW
  int32_t maxTenth = (int32_t)((maxW + 99) / 100);
  maxTenth = ((maxTenth + 4) / 5) * 5;
  if (maxTenth < 10) {
    maxTenth = 10;
  }
  lv_chart_set_range(energyObj.chart_power, LV_CHART_AXIS_PRIMARY_Y, 0, maxTenth);
  refreshYTicks(maxTenth);

  const int step = ENERGY_MINUTES_PER_DAY / kChartPoints;
  for (int i = 0; i < kChartPoints; ++i) {
    uint32_t sum = 0;
    uint32_t n = 0;
    for (int j = 0; j < step; ++j) {
      const uint16_t w = samples[i * step + j];
      if (w > 0) {
        sum += w;
        ++n;
      }
    }
    if (n == 0) {
      lv_chart_set_value_by_id(energyObj.chart_power, energyObj.ser_power, i,
                               LV_CHART_POINT_NONE);
    } else {
      const uint32_t avgW = sum / n;
      const int32_t v = (int32_t)((avgW + 50) / 100);  // 0,1 kW
      lv_chart_set_value_by_id(energyObj.chart_power, energyObj.ser_power, i, v);
    }
  }
  lv_chart_refresh(energyObj.chart_power);

  char dayBuf[48];
  if (ymd > 0) {
    snprintf(dayBuf, sizeof(dayBuf), "%02d.%02d.%04d", ymd % 100,
             (ymd / 100) % 100, ymd / 10000);
  } else if (s_dayOffset == 0) {
    snprintf(dayBuf, sizeof(dayBuf), "Dnes");
  } else {
    snprintf(dayBuf, sizeof(dayBuf), "-%d d", s_dayOffset);
  }
  setLabelIfChanged(energyObj.lbl_day, dayBuf);

  char kwhBuf[48];
  snprintf(kwhBuf, sizeof(kwhBuf), "Den: %.2f kWh", (double)dayKwh);
  setLabelIfChanged(energyObj.lbl_day_kwh, kwhBuf);

  const int wd = weekdayFromYmd(ymd);
  refreshBands(wd);
  refreshUtlumAxis(wd);
}

void refreshMonthChart() {
  if (!energyObj.ser_month || !energyObj.chart_month) {
    return;
  }
  float maxK = 1.0f;
  float vals[ENERGY_SEASON_MONTHS];
  for (int i = 0; i < ENERGY_SEASON_MONTHS; ++i) {
    vals[i] = climateEnergySeasonMonthKwh(i);
    if (vals[i] > maxK) {
      maxK = vals[i];
    }
  }
  const int32_t maxTenth = (int32_t)(maxK * 10.0f + 1.0f);
  lv_chart_set_range(energyObj.chart_month, LV_CHART_AXIS_PRIMARY_Y, 0, maxTenth);
  for (int i = 0; i < ENERGY_SEASON_MONTHS; ++i) {
    const int32_t v = (int32_t)(vals[i] * 10.0f + 0.5f);
    lv_chart_set_value_by_id(energyObj.chart_month, energyObj.ser_month, i, v);
  }
  lv_chart_refresh(energyObj.chart_month);

  const int chartX = (int)lv_obj_get_x(energyObj.chart_month);
  const int chartY = (int)lv_obj_get_y(energyObj.chart_month);
  const int chartW = (int)lv_obj_get_width(energyObj.chart_month);
  const int chartH = (int)lv_obj_get_height(energyObj.chart_month);
  const int pad = 8;
  const int plotW = chartW - 2 * pad;
  const int plotH = chartH - 2 * pad;
  const int slotW = plotW / ENERGY_SEASON_MONTHS;

  for (int i = 0; i < ENERGY_SEASON_MONTHS; ++i) {
    if (!s_lblMonthVal[i]) {
      continue;
    }
    char buf[16];
    if (vals[i] >= 100.0f) {
      snprintf(buf, sizeof(buf), "%.0f", (double)vals[i]);
    } else {
      snprintf(buf, sizeof(buf), "%.1f", (double)vals[i]);
    }
    setLabelIfChanged(s_lblMonthVal[i], buf);

    const float frac = (maxK > 0.001f) ? (vals[i] / maxK) : 0.0f;
    int ly = chartY + pad + (int)((1.0f - frac) * (float)plotH) - 26;
    if (ly < 4) {
      ly = 4;
    }
    lv_obj_set_pos(s_lblMonthVal[i], chartX + pad + i * slotW, ly);
    lv_obj_set_width(s_lblMonthVal[i], slotW);
  }
}

void refreshSummaryAndYears() {
  char buf[160];
  if (climateEnergyIsOk()) {
    snprintf(buf, sizeof(buf),
             "Příkon %u W   Dnes %.2f kWh   Měsíc %.1f kWh   Rok %.1f kWh",
             (unsigned)climateEnergyPowerW(),
             (double)climateEnergyTodayKwh(),
             (double)climateEnergyMonthKwh(),
             (double)climateEnergyYearKwh());
  } else {
    snprintf(buf, sizeof(buf), "Čekám na měřič PZEM...");
  }
  setLabelIfChanged(energyObj.lbl_summary, buf);

  char ybuf[192];
  size_t n = 0;
  n += (size_t)snprintf(ybuf + n, sizeof(ybuf) - n, "Roky: ");
  bool any = false;
  for (int i = 0; i < ENERGY_YEAR_SLOTS; ++i) {
    int year = 0;
    float kwh = 0.0f;
    if (!climateEnergyYearGet(i, &year, &kwh)) {
      continue;
    }
    any = true;
    n += (size_t)snprintf(ybuf + n, sizeof(ybuf) - n, "%d %.1f  ", year,
                          (double)kwh);
  }
  if (!any) {
    snprintf(ybuf, sizeof(ybuf), "Roky: -");
  }
  setLabelIfChanged(energyObj.lbl_years, ybuf);
}

}  // namespace

void uiEnergyCreate(void) {
  if (s_created) {
    return;
  }
  memset(&energyObj, 0, sizeof(energyObj));
  memset(s_lblHour, 0, sizeof(s_lblHour));
  memset(s_lblYTick, 0, sizeof(s_lblYTick));
  memset(s_lblMonthVal, 0, sizeof(s_lblMonthVal));
  memset(s_lblMonthName, 0, sizeof(s_lblMonthName));

  energyObj.screen = lv_obj_create(nullptr);
  lv_obj_set_size(energyObj.screen, kW, kH);
  lv_obj_set_style_bg_color(energyObj.screen, lv_color_hex(kColBg),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(energyObj.screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(energyObj.screen, LV_OBJ_FLAG_SCROLLABLE);

  energyObj.btn_back =
      makeButton(energyObj.screen, kMargin, kMargin, 100, kBtnH, "Zpět", onBack,
                 kColAccent);
  energyObj.lbl_title =
      makeLabel(energyObj.screen, kMargin + 110, kMargin + 8, 400, "Spotřeba TČ",
                kColText);
  energyObj.lbl_summary =
      makeLabel(energyObj.screen, kMargin, kMargin + kBtnH + 4, kW - 2 * kMargin,
                "---", kColMuted);

  const int yearsH = 40;
  const int topY = kMargin + kBtnH + 4 + 30;
  const int bottomY = kH - kMargin - yearsH;
  const int stackH = bottomY - topY - kGap;
  const int powerH = stackH * 58 / 100;
  const int monthH = stackH - powerH - kGap;
  const int fullW = kW - 2 * kMargin;

  s_powerPanel = makePanel(energyObj.screen, kMargin, topY, fullW, powerH);
  energyObj.btn_day_prev =
      makeButton(s_powerPanel, kPad, kPad, 56, 36, "<", onDayPrev, kColPurple);
  energyObj.btn_day_next =
      makeButton(s_powerPanel, kPad + 64, kPad, 56, 36, ">", onDayNext, kColPurple);
  energyObj.lbl_day =
      makeLabel(s_powerPanel, kPad + 140, kPad + 6, 220, "Dnes", kColText);
  energyObj.lbl_day_kwh =
      makeLabel(s_powerPanel, fullW - 280, kPad + 6, 260, "Den: —", kColOrange);
  energyObj.lbl_y_unit =
      makeLabel(s_powerPanel, kPad, kPad + 40, kYAxisW, "kW", kColMuted);

  const int chartY = kPad + 44;
  const int chartInnerH = powerH - chartY - kPad - kHourH - kUtlumStripH - 4;
  const int chartInnerW = fullW - 2 * kPad - kYAxisW;
  const int chartX = kPad + kYAxisW;

  for (int i = 0; i < kYTicks; ++i) {
    s_lblYTick[i] =
        makeLabel(s_powerPanel, kPad, chartY, kYAxisW - 4, "0.0", kColMuted);
    lv_obj_set_style_text_align(s_lblYTick[i], LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  energyObj.band_layer = lv_obj_create(s_powerPanel);
  lv_obj_set_pos(energyObj.band_layer, chartX, chartY);
  lv_obj_set_size(energyObj.band_layer, chartInnerW, chartInnerH);
  lv_obj_set_style_bg_opa(energyObj.band_layer, LV_OPA_TRANSP,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(energyObj.band_layer, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(energyObj.band_layer, 0,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(energyObj.band_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(energyObj.band_layer, LV_OBJ_FLAG_CLICKABLE);

  energyObj.chart_power = lv_chart_create(s_powerPanel);
  lv_obj_set_pos(energyObj.chart_power, chartX, chartY);
  lv_obj_set_size(energyObj.chart_power, chartInnerW, chartInnerH);
  lv_obj_set_style_bg_opa(energyObj.chart_power, LV_OPA_TRANSP,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(energyObj.chart_power, 0,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_chart_set_type(energyObj.chart_power, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(energyObj.chart_power, kChartPoints);
  lv_chart_set_range(energyObj.chart_power, LV_CHART_AXIS_PRIMARY_Y, 0, 10);
  lv_chart_set_div_line_count(energyObj.chart_power, kYTicks - 1, 12);
  lv_obj_set_style_line_width(energyObj.chart_power, 3, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(energyObj.chart_power, LV_OPA_0, LV_PART_ITEMS);
  energyObj.ser_power = lv_chart_add_series(
      energyObj.chart_power, lv_color_hex(kColGreen), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < kChartPoints; ++i) {
    lv_chart_set_value_by_id(energyObj.chart_power, energyObj.ser_power, i,
                             LV_CHART_POINT_NONE);
  }

  const int hourY = chartY + chartInnerH + 2;
  const int hourW = chartInnerW / 24;
  for (int h = 0; h < 24; ++h) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", h + 1);
    s_lblHour[h] = makeLabel(s_powerPanel, chartX + h * hourW, hourY, hourW, buf,
                             kColMuted);
    lv_obj_set_style_text_align(s_lblHour[h], LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  s_utlumStrip = lv_obj_create(s_powerPanel);
  lv_obj_set_pos(s_utlumStrip, chartX, hourY + kHourH - 2);
  lv_obj_set_size(s_utlumStrip, chartInnerW, kUtlumStripH);
  lv_obj_set_style_bg_color(s_utlumStrip, lv_color_hex(0x2A2A30u),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(s_utlumStrip, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(s_utlumStrip, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(s_utlumStrip, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(s_utlumStrip, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(s_utlumStrip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_utlumStrip, LV_OBJ_FLAG_CLICKABLE);

  s_monthPanel =
      makePanel(energyObj.screen, kMargin, topY + powerH + kGap, fullW, monthH);
  makeLabel(s_monthPanel, kPad, 6, fullW - 2 * kPad, "Topná sezóna září - květen",
            kColText);

  const int mChartY = 36;
  const int mNameH = 28;
  const int mChartH = monthH - mChartY - mNameH - kPad;
  energyObj.chart_month = lv_chart_create(s_monthPanel);
  lv_obj_set_pos(energyObj.chart_month, kPad, mChartY);
  lv_obj_set_size(energyObj.chart_month, fullW - 2 * kPad, mChartH);
  lv_obj_set_style_pad_all(energyObj.chart_month, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_chart_set_type(energyObj.chart_month, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(energyObj.chart_month, ENERGY_SEASON_MONTHS);
  lv_chart_set_range(energyObj.chart_month, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  energyObj.ser_month = lv_chart_add_series(
      energyObj.chart_month, lv_color_hex(kColAccent), LV_CHART_AXIS_PRIMARY_Y);

  const int cellW = (fullW - 2 * kPad) / ENERGY_SEASON_MONTHS;
  for (int i = 0; i < ENERGY_SEASON_MONTHS; ++i) {
    s_lblMonthVal[i] = makeLabel(s_monthPanel, kPad + i * cellW, mChartY, cellW,
                                 "0", kColOrange);
    lv_obj_set_style_text_font(s_lblMonthVal[i], kFontCount,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(s_lblMonthVal[i], LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    s_lblMonthName[i] =
        makeLabel(s_monthPanel, kPad + i * cellW, monthH - mNameH, cellW,
                  kMonthNames[i], kColMuted);
    lv_obj_set_style_text_align(s_lblMonthName[i], LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  energyObj.lbl_years =
      makeLabel(energyObj.screen, kMargin, kH - kMargin - yearsH + 4,
                kW - 2 * kMargin, "Roky: -", kColText);
  lv_obj_set_style_text_font(energyObj.lbl_years, kFontCount,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(energyObj.lbl_day_kwh, kFontCount,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  s_created = true;
  s_lastGen = 0;
  uiEnergyTick();
}

void uiEnergyEnsureCreated(void) {
  if (!s_created) {
    uiEnergyCreate();
  }
}

void uiEnergyTick(void) {
  if (!s_created || !energyObj.screen) {
    return;
  }
  if (!uiIsEnergyScreen()) {
    return;
  }
  const uint32_t gen = climateEnergyHistoryGen();
  if (gen == s_lastGen) {
    refreshSummaryAndYears();
    return;
  }
  s_lastGen = gen;
  refreshSummaryAndYears();
  refreshPowerChart();
  refreshMonthChart();
}

lv_obj_t* uiEnergyScreen(void) {
  uiEnergyEnsureCreated();
  return energyObj.screen;
}
