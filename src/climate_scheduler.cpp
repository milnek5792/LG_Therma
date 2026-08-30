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
  char line[96];
  if (!uiEez.cas_platny) {
    snprintf(line, sizeof(line),
             "Tichy rezim T/C: noc %02d:00-%02d:00 (ceka NTP)",
             CLIMATE_TICHY_NOC_START_H, CLIMATE_TICHY_NOC_END_H);
  } else if (autoMode && !s_manualOverride) {
    snprintf(line, sizeof(line),
             "Tichy rezim T/C: %s | noc %02d:00-%02d:00 | dalsi zmena %02d:00",
             tichyActive ? "ZAP" : "VYP",
             CLIMATE_TICHY_NOC_START_H, CLIMATE_TICHY_NOC_END_H,
             tichyActive ? CLIMATE_TICHY_NOC_END_H : CLIMATE_TICHY_NOC_START_H);
  } else {
    snprintf(line, sizeof(line),
             "Tichy rezim T/C: %s | rucne (planovac pozdeji)",
             tichyActive ? "ZAP" : "VYP");
    (void)hour;
    (void)minute;
  }
  strncpy(uiEez.plan_text, line, sizeof(uiEez.plan_text));
  uiEez.plan_text[sizeof(uiEez.plan_text) - 1] = '\0';
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
