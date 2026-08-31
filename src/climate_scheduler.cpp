#include "climate_scheduler.h"

#include "bus_lg_config.h"
#include "ui_eez_model.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {

bool s_manualOverride = false;
bool s_manualValue = false;

bool isNightHour(int hour) {
  if (CLIMATE_TICHY_NOC_START_H == CLIMATE_TICHY_NOC_END_H) {
    return false;
  }
  if (CLIMATE_TICHY_NOC_START_H < CLIMATE_TICHY_NOC_END_H) {
    return hour >= CLIMATE_TICHY_NOC_START_H && hour < CLIMATE_TICHY_NOC_END_H;
  }
  return hour >= CLIMATE_TICHY_NOC_START_H || hour < CLIMATE_TICHY_NOC_END_H;
}

void updatePlanText(int hour, int minute, bool tichyActive, bool autoMode) {
  (void)hour;
  (void)minute;
  (void)tichyActive;
  (void)autoMode;
  // Stav týdenního plánu vlastní climate_plan (plan_text / plan_title).
  // Tichý režim je na tlačítku „Tichý režim“ — neprepisovat plan_text (blikání).
}

}  // namespace

void climateSchedulerInit() {
  s_manualOverride = false;
  s_manualValue = false;
  updatePlanText(0, 0, false, false);
}

void climateSchedulerTick() {
  if (!uiEez.cas_platny) {
    if (!s_manualOverride) {
      uiEez.sig_utlum = false;
    }
    updatePlanText(0, 0, uiEez.sig_utlum, false);
    return;
  }

  time_t now = time(nullptr);
  struct tm tmLocal;
  localtime_r(&now, &tmLocal);

  if (s_manualOverride) {
    uiEez.sig_utlum = s_manualValue;
    updatePlanText(tmLocal.tm_hour, tmLocal.tm_min, uiEez.sig_utlum, false);
    return;
  }

#if CLIMATE_TICHY_AUTO_NOC
  uiEez.sig_utlum = isNightHour(tmLocal.tm_hour);
  updatePlanText(tmLocal.tm_hour, tmLocal.tm_min, uiEez.sig_utlum, true);
#else
  updatePlanText(tmLocal.tm_hour, tmLocal.tm_min, uiEez.sig_utlum, false);
#endif
}

void climateTichyManualToggle() {
  s_manualOverride = true;
  s_manualValue = !uiEez.sig_utlum;
  uiEez.sig_utlum = s_manualValue;
  Serial.printf("[T/C] tichy rezim rucne: %s\n", s_manualValue ? "ZAP" : "VYP");
}
