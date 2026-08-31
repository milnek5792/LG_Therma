// climate_regulator.cpp — ekviterm (lineární −15/+15) + PI korekce + asym. Eco
#include "climate_regulator.h"

#include "climate_room.h"
#include "bus_lg_model.h"
#include "storage_config_nvs.h"
#include "ui_bus_bindings.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

namespace {

static const char* TAG = "REG";

constexpr uint32_t kPidPeriodMs = 120000;
constexpr uint32_t kHistoryPeriodMs = 120000;
constexpr uint32_t kBusRetryMs = 5000;

constexpr float kOutMin = (float)REG_T_WATER_MIN_C;
constexpr float kOutMax = (float)REG_T_WATER_MAX_C;
constexpr float kFixedBaseC = 32.5f;

RegulatorConfig s_cfg;
float s_planRoomOffset = 0.0f;
bool s_planStop = false;

float s_iTerm = 0.0f;
float s_prevError = 0.0f;
float s_tWater = kFixedBaseC;
float s_tWaterRaw = kFixedBaseC;
float s_eqBase = kFixedBaseC;
float s_pidCorr = 0.0f;
float s_lastP = 0.0f;
float s_lastError = 0.0f;
bool s_havePrev = false;
bool s_eco = false;
bool s_cfgSavePending = false;
bool s_haveEqBase = false;
uint8_t s_lastWrittenC = 0;
bool s_haveWritten = false;
uint32_t s_lastPidMs = 0;
uint32_t s_lastWriteMs = 0;
uint32_t s_lastHistoryMs = 0;
bool s_wasAuto = false;

RegulatorHistoryPoint s_hist[REG_HISTORY_LEN];
int s_histCount = 0;
int s_histHead = 0;
uint32_t s_histGen = 0;

float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

void pushHistory(float room, float sp, float tWater, float outdoor) {
  s_hist[s_histHead].room_c = room;
  s_hist[s_histHead].room_sp_c = sp;
  s_hist[s_histHead].t_water_c = tWater;
  s_hist[s_histHead].outdoor_c = outdoor;
  s_histHead = (s_histHead + 1) % REG_HISTORY_LEN;
  if (s_histCount < REG_HISTORY_LEN) {
    s_histCount++;
  }
  s_histGen++;
}

uint8_t waterToBusC(float tC, float err) {
  int c = (err < -0.05f) ? (int)floorf(tC + 0.001f) : (int)lroundf(tC);
  if (c < REG_T_WATER_MIN_C) {
    c = REG_T_WATER_MIN_C;
  }
  if (c > REG_T_WATER_MAX_C) {
    c = REG_T_WATER_MAX_C;
  }
  return (uint8_t)c;
}

float currentOutputWaterC(void) {
  const uint8_t a0Sp = lgModelA0Bajt(8);
  if (lgMaCerstoA0() && a0Sp >= REG_T_WATER_MIN_C && a0Sp <= REG_T_WATER_MAX_C) {
    return (float)a0Sp;
  }
  if (s_tWater >= kOutMin && s_tWater <= kOutMax) {
    return s_tWater;
  }
  return kFixedBaseC;
}

float equithermWaterFromCfg(float outdoorC, const RegulatorConfig* cfg) {
  if (!cfg) {
    return kFixedBaseC;
  }
  const float tCold = cfg->t_water_cold_c;
  const float tWarm = cfg->t_water_warm_c;
  const float outCold = REG_EQ_OUT_COLD_C;
  const float outWarm = REG_EQ_OUT_WARM_C;
  float t;
  if (outdoorC <= outCold) {
    t = tCold;
  } else if (outdoorC >= outWarm) {
    t = tWarm;
  } else {
    const float f = (outdoorC - outCold) / (outWarm - outCold);
    t = tCold + f * (tWarm - tCold);
  }
  return t + cfg->offset_c;
}

float computeEqBase(float outdoorC) {
  return equithermWaterFromCfg(outdoorC, &s_cfg);
}

float resolveBase(bool outOk, float outC) {
  if (s_cfg.use_equitherm == 0) {
    return kFixedBaseC;
  }
  if (outOk && !isnan(outC)) {
    s_eqBase = computeEqBase(outC);
    s_haveEqBase = true;
    return s_eqBase;
  }
  if (s_haveEqBase) {
    return s_eqBase;
  }
  return kFixedBaseC;
}

void adoptOutput(const char* why) {
  const bool outOk = climateRoomOutdoorIsOk();
  const float outC = outOk ? climateRoomOutdoorTempC() : NAN;
  const float base = resolveBase(outOk, outC);
  const float a0 = currentOutputWaterC();
  s_eqBase = base;
  s_pidCorr = clampf(a0 - base, REG_CORR_MIN_C, REG_CORR_MAX_C);
  s_iTerm = s_pidCorr;
  s_tWater = clampf(base + s_pidCorr, kOutMin, kOutMax);
  s_tWaterRaw = s_tWater;
  s_lastP = 0.0f;
  s_prevError = 0.0f;
  s_havePrev = false;
  s_lastError = 0.0f;
  ESP_LOGI(TAG, "adopt base=%.1f corr=%.1f T=%.1f I=%.1f — %s", (double)base,
           (double)s_pidCorr, (double)s_tWater, (double)s_iTerm,
           why ? why : "?");
}

bool tcBeziProRegulaci(void) {
  return cilovyZapnutoTab5 || tcPozadavekZap || cekameNaOrigStart;
}

void requestWaterSp(uint8_t tC, uint32_t now) {
  ESP_LOGI(TAG, "Auto SP vody -> %u C (T=%.1f) pending=%u a0=%u",
           (unsigned)tC, (double)s_tWater, (unsigned)uiEez.sp_pending,
           (unsigned)lgModelA0Bajt(8));
  uiBusSetSetpointC(tC);
  s_lastWrittenC = tC;
  s_lastWriteMs = now;
}

void maybeWriteSetpoint(uint8_t tC, uint32_t now) {
  if (s_planStop) {
    return;
  }
  if (!tcBeziProRegulaci()) {
    return;
  }

  const uint8_t a0Sp = lgModelA0Bajt(8);
  const bool a0Match = (a0Sp >= 15 && a0Sp <= 65 && a0Sp == tC);
  const bool pendingSame = (uiEez.sp_pending == tC);

  if (a0Match && uiEez.sp_pending == 0) {
    s_haveWritten = true;
    s_lastWrittenC = tC;
    return;
  }

  if (pendingSame && (now - s_lastWriteMs) < kBusRetryMs) {
    return;
  }

  requestWaterSp(tC, now);
}

void retryUnconfirmedSp(uint32_t now) {
  if (s_planStop || !tcBeziProRegulaci()) {
    return;
  }
  if (uiEez.sp_pending == 0) {
    return;
  }
  if (s_lastWriteMs != 0 && (now - s_lastWriteMs) < kBusRetryMs) {
    return;
  }
  requestWaterSp(uiEez.sp_pending, now);
}

void migrateToEquithermGains(void) {
  // Starý PID na vodu měl Kp ~12.5; nový ekviterm+PI používá Kp≈4.
  if (s_cfg.kp < 12.0f) {
    return;
  }
  ESP_LOGW(TAG, "NVS Kp=%.1f (stary PID vody) — Kp=4 Ki=0.1 + ekviterm",
           (double)s_cfg.kp);
  s_cfg.kp = 4.0f;
  s_cfg.ki = 0.1f;
  s_cfg.kd = 0.0f;
  s_cfg.use_equitherm = 1;
  climateRegulatorSave();
}

}  // namespace

void climateRegulatorSetDefaults(RegulatorConfig* cfg) {
  if (!cfg) {
    return;
  }
  cfg->room_sp_c = 22.0f;
  cfg->t_water_cold_c = REG_EQ_WATER_COLD_DEFAULT_C;
  cfg->t_water_warm_c = REG_EQ_WATER_WARM_DEFAULT_C;
  cfg->offset_c = 0.0f;
  cfg->kp = 4.0f;
  cfg->ki = 0.1f;
  cfg->kd = 0.0f;
  cfg->trim_limit_pct = 25.0f;
  cfg->deadband_c = 0.2f;
  cfg->use_equitherm = 1;
  cfg->_pad[0] = cfg->_pad[1] = cfg->_pad[2] = 0;
}

void climateRegulatorSetUseEquitherm(bool on) {
  s_cfg.use_equitherm = on ? 1 : 0;
  climateRegulatorRequestSave();
  ESP_LOGI(TAG, "use_equitherm=%d", (int)s_cfg.use_equitherm);
}

bool climateRegulatorUseEquitherm(void) { return s_cfg.use_equitherm != 0; }

bool climateRegulatorIsEcoMode(void) { return s_eco; }

float climateRegulatorEquithermWaterAt(float outdoorC) {
  return equithermWaterFromCfg(outdoorC, &s_cfg);
}

void climateRegulatorInit(void) {
  climateRegulatorSetDefaults(&s_cfg);
  if (!storageLoadRegulatorConfig(&s_cfg)) {
    ESP_LOGW(TAG, "NVS reg_cfg chybi — vychozi gainy");
    climateRegulatorSetDefaults(&s_cfg);
    climateRegulatorSave();
  } else {
    ESP_LOGI(TAG,
             "NVS reg_cfg OK Kp=%.1f Ki=%.2f Kd=%.1f voda %.0f/%.0f offset=%+.0f",
             (double)s_cfg.kp, (double)s_cfg.ki, (double)s_cfg.kd,
             (double)s_cfg.t_water_cold_c, (double)s_cfg.t_water_warm_c,
             (double)s_cfg.offset_c);
    migrateToEquithermGains();
  }
  s_cfg.room_sp_c = clampf(s_cfg.room_sp_c, 18.0f, 24.0f);
  s_cfg.t_water_cold_c =
      clampf(s_cfg.t_water_cold_c, kOutMin, kOutMax);
  s_cfg.t_water_warm_c =
      clampf(s_cfg.t_water_warm_c, kOutMin, kOutMax);
  s_cfg.offset_c =
      clampf(s_cfg.offset_c, REG_EQ_OFFSET_MIN_C, REG_EQ_OFFSET_MAX_C);
  s_cfg.kp = clampf(s_cfg.kp, 0.0f, 20.0f);
  s_cfg.ki = clampf(s_cfg.ki, 0.0f, 5.0f);
  s_cfg.kd = clampf(s_cfg.kd, 0.0f, 20.0f);
  s_cfg.use_equitherm = s_cfg.use_equitherm ? 1 : 0;
  s_planRoomOffset = 0.0f;
  s_planStop = false;
  s_lastPidMs = 0;
  s_histCount = 0;
  s_histHead = 0;
  s_histGen = 0;
  s_wasAuto = false;
  s_eco = false;
  s_haveEqBase = false;
  adoptOutput("init");
  ESP_LOGI(TAG,
           "init ekviterm+PI room_sp=%.1f Kp=%.1f Ki=%.2f ekv=%d period=%lu ms",
           (double)s_cfg.room_sp_c, (double)s_cfg.kp, (double)s_cfg.ki,
           (int)s_cfg.use_equitherm, (unsigned long)kPidPeriodMs);
}

void climateRegulatorSave(void) {
  storageSaveRegulatorConfig(&s_cfg);
  s_cfgSavePending = false;
  ESP_LOGI(TAG, "NVS reg_cfg ulozeno Kp=%.1f Ki=%.2f", (double)s_cfg.kp,
           (double)s_cfg.ki);
}

void climateRegulatorRequestSave(void) {
  s_cfgSavePending = true;
}

void climateRegulatorFlushPendingSave(void) {
  if (s_cfgSavePending) {
    climateRegulatorSave();
    s_cfgSavePending = false;
  }
}

const RegulatorConfig* climateRegulatorGetConfig(void) { return &s_cfg; }

RegulatorConfig* climateRegulatorGetConfigMutable(void) { return &s_cfg; }

void climateRegulatorAdjustRoomSp(float deltaC) {
  climateRegulatorSetRoomSp(s_cfg.room_sp_c + deltaC);
}

void climateRegulatorSetRoomSp(float c) {
  c = roundf(c * 2.0f) / 2.0f;
  s_cfg.room_sp_c = clampf(c, 18.0f, 24.0f);
  climateRegulatorRequestSave();
}

float climateRegulatorRoomSpEffective(void) {
  return clampf(s_cfg.room_sp_c + s_planRoomOffset, 16.0f, 24.0f);
}

void climateRegulatorSetPlanRoomOffset(float offsetC) {
  s_planRoomOffset = clampf(offsetC, -5.0f, 0.0f);
}

float climateRegulatorPlanRoomOffset(void) { return s_planRoomOffset; }

void climateRegulatorSetPlanStop(bool stop) { s_planStop = stop; }

bool climateRegulatorPlanRequestsStop(void) { return s_planStop; }

void climateRegulatorGetSnapshot(RegulatorSnapshot* out) {
  if (!out) {
    return;
  }
  out->t_water_sp_c = s_tWater;
  out->t_water_raw_c = s_tWaterRaw;
  out->eq_base_c = s_eqBase;
  out->pid_corr_c = s_pidCorr;
  out->p_term_c = s_lastP;
  out->i_term_c = s_iTerm;
  out->error_c = s_lastError;
  out->room_sp_c = climateRegulatorRoomSpEffective();
  out->room_c = climateRoomIsOk() ? climateRoomTempC() : UI_TEPLOTA_NEPLATNA;
  out->outdoor_c =
      climateRoomOutdoorIsOk() ? climateRoomOutdoorTempC() : UI_TEPLOTA_NEPLATNA;
  out->t_water_c = waterToBusC(s_tWater, s_eco ? -1.0f : s_lastError);
  out->active = (uiEez.rezim == UI_REZIM_AUTO);
  out->eco_mode = s_eco;
  out->room_ok = climateRoomIsOk();
  out->outdoor_ok = climateRoomOutdoorIsOk();
  out->use_equitherm = (s_cfg.use_equitherm != 0);
  out->pid_period_ms = kPidPeriodMs;
  if (s_lastPidMs == 0) {
    out->ms_since_pid = UINT32_MAX;
    out->ms_to_next_pid = 0;
  } else {
    const uint32_t now = millis();
    const uint32_t since = now - s_lastPidMs;
    out->ms_since_pid = since;
    out->ms_to_next_pid = (since >= kPidPeriodMs) ? 0 : (kPidPeriodMs - since);
  }
}

int climateRegulatorHistoryCount(void) { return s_histCount; }

uint32_t climateRegulatorHistoryGen(void) { return s_histGen; }

void climateRegulatorHistoryGet(int index, RegulatorHistoryPoint* out) {
  if (!out || index < 0 || index >= s_histCount) {
    if (out) {
      memset(out, 0, sizeof(*out));
    }
    return;
  }
  const int start = (s_histCount < REG_HISTORY_LEN) ? 0 : s_histHead;
  const int i = (start + index) % REG_HISTORY_LEN;
  *out = s_hist[i];
}

void climateRegulatorTick(void) {
  const bool isAuto = (uiEez.rezim == UI_REZIM_AUTO);
  if (!isAuto) {
    if (s_wasAuto) {
      ESP_LOGI(TAG, "leave Auto — PID paused (I drzen, eco=%d)", (int)s_eco);
    }
    s_wasAuto = false;
    return;
  }
  if (!s_wasAuto) {
    ESP_LOGI(TAG, "enter Auto");
    s_haveWritten = false;
    s_lastPidMs = 0;
    adoptOutput("enter Auto");
  }
  s_wasAuto = true;

  if (!tcBeziProRegulaci()) {
    return;
  }

  const uint32_t now = millis();
  retryUnconfirmedSp(now);

  if (s_lastPidMs != 0 && (now - s_lastPidMs) < kPidPeriodMs) {
    return;
  }
  s_lastPidMs = now;

  const float roomSp = climateRegulatorRoomSpEffective();
  const bool roomOk = climateRoomIsOk();
  const float roomC = roomOk ? climateRoomTempC() : NAN;
  const bool outOk = climateRoomOutdoorIsOk();
  const float outC = outOk ? climateRoomOutdoorTempC() : NAN;

  float err = 0.0f;
  const float base = resolveBase(outOk, outC);
  s_eqBase = base;

  if (roomOk && !isnan(roomC)) {
    err = roomSp - roomC;
    s_lastError = err;

    if (roomC >= (roomSp + REG_ECO_HYST_C)) {
      if (!s_eco) {
        ESP_LOGI(TAG, "Eco ON — I zmrazen=%.2f (room=%.1f >= sp+%.1f)",
                 (double)s_iTerm, (double)roomC, (double)REG_ECO_HYST_C);
      }
      s_eco = true;
    } else if (roomC <= roomSp) {
      if (s_eco) {
        ESP_LOGI(TAG, "Eco OFF — navrat AUTOMATIC I=%.2f", (double)s_iTerm);
      }
      s_eco = false;
    }

    if (s_eco) {
      s_lastP = 0.0f;
      s_pidCorr = s_iTerm;
      s_tWater = kOutMin;
      s_tWaterRaw = kOutMin;
      ESP_LOGI(TAG, "Eco mod — SP vody 20 C (kompresor off) I=%.2f e=%.2f",
               (double)s_iTerm, (double)err);
    } else {
      float d = 0.0f;
      if (s_havePrev) {
        d = s_cfg.kd * (err - s_prevError);
      }
      s_prevError = err;
      s_havePrev = true;

      s_iTerm += s_cfg.ki * err;
      const float p = s_cfg.kp * err;
      s_lastP = p;
      float corr = p + s_iTerm + d;
      corr = clampf(corr, REG_CORR_MIN_C, REG_CORR_MAX_C);
      s_iTerm = corr - p - d;
      s_iTerm = clampf(s_iTerm, REG_CORR_MIN_C, REG_CORR_MAX_C);
      s_pidCorr = corr;

      const float tRaw = base + corr;
      const float t = clampf(tRaw, kOutMin, kOutMax);
      s_tWaterRaw = tRaw;
      s_tWater = t;

      ESP_LOGI(TAG,
               "Bezna regulace — zaklad=%.1f korekce=%.2f (P=%.1f I=%.1f) "
               "req=%.1f e=%.2f room=%.1f/%.1f out=%.1f",
               (double)base, (double)corr, (double)p, (double)s_iTerm,
               (double)t, (double)err, (double)roomC, (double)roomSp,
               outOk ? (double)outC : -999.0);
    }
  } else {
    s_lastError = 0.0f;
    s_lastP = 0.0f;
    s_pidCorr = clampf(s_iTerm, REG_CORR_MIN_C, REG_CORR_MAX_C);
    s_tWater = clampf(base + s_pidCorr, kOutMin, kOutMax);
    s_tWaterRaw = s_tWater;
    ESP_LOGW(TAG, "room senzor OFF — req=%.1f (base=%.1f + I=%.1f)",
             (double)s_tWater, (double)base, (double)s_iTerm);
  }

  const uint8_t tBus = waterToBusC(s_tWater, s_eco ? -1.0f : err);

  if (!s_planStop) {
    maybeWriteSetpoint(tBus, now);
  }

  if (s_lastHistoryMs == 0 || (now - s_lastHistoryMs) >= kHistoryPeriodMs) {
    s_lastHistoryMs = now;
    pushHistory(roomOk ? roomC : UI_TEPLOTA_NEPLATNA, roomSp, s_tWater,
                outOk ? outC : UI_TEPLOTA_NEPLATNA);
  }
}

void climateRegulatorRequestImmediateTick(void) {
  s_lastPidMs = 0;
}
