// climate_regulator.cpp — ekviterm (BLE outdoor) + PID trim → SP vody 20–45 °C
#include "climate_regulator.h"

#include "climate_ble_room.h"
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

constexpr uint32_t kPidPeriodMs = 120000;      // ~BLE poll
constexpr uint32_t kHistoryPeriodMs = 120000;
/** Opakovaný zápis jen při neshodě se sběrnicí (stejný cíl už jsme poslali). */
constexpr uint32_t kBusRetryMs = 5000;
constexpr float kAdaptGain = 0.02f;               // bias % / °C chyby za periodu
constexpr float kAdaptLimit = 15.0f;
/** Při přetopení (err < 0) silnější P, ať se voda snižuje hned. */
constexpr float kCoolKpScale = 2.0f;

RegulatorConfig s_cfg;
float s_planRoomOffset = 0.0f;
bool s_planStop = false;

float s_integral = 0.0f;
float s_prevError = 0.0f;
float s_u = 0.0f;
float s_uBase = 0.0f;
float s_uTrim = 0.0f;
float s_lastError = 0.0f;
bool s_havePrev = false;
uint8_t s_lastWrittenC = 0;
bool s_haveWritten = false;
uint32_t s_lastPidMs = 0;
uint32_t s_lastWriteMs = 0;
uint32_t s_lastHistoryMs = 0;
bool s_wasAuto = false;

RegulatorHistoryPoint s_hist[REG_HISTORY_LEN];
int s_histCount = 0;
int s_histHead = 0;  // next write index

float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

float equithermPct(float tOut) {
  const float tCold = s_cfg.t_out_cold_c;
  const float tWarm = s_cfg.t_out_warm_c;
  if (fabsf(tWarm - tCold) < 0.01f) {
    return s_cfg.u_cold_pct;
  }
  // tCold → u_cold, tWarm → u_warm (lineární)
  float ratio = (tOut - tCold) / (tWarm - tCold);
  ratio = clampf(ratio, 0.0f, 1.0f);
  return s_cfg.u_cold_pct + ratio * (s_cfg.u_warm_pct - s_cfg.u_cold_pct);
}

void pushHistory(float room, float sp, float u, float outdoor) {
  s_hist[s_histHead].room_c = room;
  s_hist[s_histHead].room_sp_c = sp;
  s_hist[s_histHead].u_pct = u;
  s_hist[s_histHead].outdoor_c = outdoor;
  s_histHead = (s_histHead + 1) % REG_HISTORY_LEN;
  if (s_histCount < REG_HISTORY_LEN) {
    s_histCount++;
  }
}

void resetPid() {
  s_integral = 0.0f;
  s_prevError = 0.0f;
  s_havePrev = false;
  s_uTrim = 0.0f;
}

uint8_t pctToWaterC(float uPct) {
  const float t = climateRegulatorPctToTemp(uPct);
  int c = (int)lroundf(t);
  if (c < REG_T_WATER_MIN_C) {
    c = REG_T_WATER_MIN_C;
  }
  if (c > REG_T_WATER_MAX_C) {
    c = REG_T_WATER_MAX_C;
  }
  return (uint8_t)c;
}

void maybeWriteSetpoint(uint8_t tC, uint32_t now) {
  if (s_planStop) {
    return;
  }

  // Bez běžícího T/C neposílej SP — dřívější zápis s zap=false posílal C0 VYP a shodil čerpadlo
  if (!cilovyZapnutoTab5 && !tcPozadavekZap && !cekameNaOrigStart && !stavZapnuto) {
    return;
  }

  lgModelLock();
  const uint8_t heldSp = cilovaTeplotaTab5;
  // Skutečný SP na TČ (A0 B8)
  const uint8_t a0Sp = lgModelA0Bajt(8);
  lgModelUnlock();

  const bool a0Mismatch =
      (a0Sp >= 15 && a0Sp <= 65 && a0Sp != tC);
  const bool heldMismatch =
      (heldSp >= REG_T_WATER_MIN_C && heldSp <= REG_T_WATER_MAX_C && heldSp != tC &&
       drzetStavAktivni);

  const bool newTarget = !s_haveWritten || tC != s_lastWrittenC;

  // Nová teplota z PID → LIN okamžitě (bez rate-limitu)
  if (!newTarget) {
    if (!a0Mismatch && !heldMismatch) {
      return;
    }
    // Stejný cíl, ale sběrnice/held nesedí — retry s krátkou pauzou
    if ((now - s_lastWriteMs) < kBusRetryMs) {
      return;
    }
  }

  ESP_LOGI(TAG, "Auto SP vody -> %u C (u=%.1f%%) a0=%u held=%u new=%d mismatch=%d",
           (unsigned)tC, (double)s_u, (unsigned)a0Sp, (unsigned)heldSp,
           (int)newTarget, (int)(a0Mismatch || heldMismatch));
  uiBusSetSetpointC(tC);
  s_lastWrittenC = tC;
  s_haveWritten = true;
  s_lastWriteMs = now;
}

}  // namespace

float climateRegulatorPctToTemp(float uPct) {
  const float u = clampf(uPct, 0.0f, 100.0f);
  const float span = (float)(REG_T_WATER_MAX_C - REG_T_WATER_MIN_C);
  return (float)REG_T_WATER_MIN_C + u * (span / 100.0f);
}

float climateRegulatorTempToPct(float tC) {
  const float span = (float)(REG_T_WATER_MAX_C - REG_T_WATER_MIN_C);
  return clampf((tC - (float)REG_T_WATER_MIN_C) / (span / 100.0f), 0.0f, 100.0f);
}

void climateRegulatorSetDefaults(RegulatorConfig* cfg) {
  if (!cfg) {
    return;
  }
  cfg->room_sp_c = 22.0f;
  cfg->t_out_cold_c = -15.0f;
  cfg->u_cold_pct = 100.0f;
  cfg->t_out_warm_c = 15.0f;
  cfg->u_warm_pct = 0.0f;
  cfg->kp = 8.0f;    // % / °C
  cfg->ki = 0.15f;   // % / (°C·perioda)
  cfg->kd = 2.0f;
  cfg->bias_pct = 0.0f;
  cfg->trim_limit_pct = 25.0f;
  cfg->deadband_c = 0.2f;
  cfg->use_equitherm = 1;
  cfg->_pad[0] = cfg->_pad[1] = cfg->_pad[2] = 0;
}

void climateRegulatorSetUseEquitherm(bool on) {
  s_cfg.use_equitherm = on ? 1 : 0;
  resetPid();
  s_haveWritten = false;
  ESP_LOGI(TAG, "use_equitherm=%d", (int)s_cfg.use_equitherm);
}

bool climateRegulatorUseEquitherm(void) { return s_cfg.use_equitherm != 0; }

void climateRegulatorInit(void) {
  climateRegulatorSetDefaults(&s_cfg);
  if (!storageLoadRegulatorConfig(&s_cfg)) {
    climateRegulatorSetDefaults(&s_cfg);
    climateRegulatorSave();
  }
  // sanitize
  s_cfg.room_sp_c = clampf(s_cfg.room_sp_c, 18.0f, 24.0f);
  s_cfg.trim_limit_pct = clampf(s_cfg.trim_limit_pct, 5.0f, 40.0f);
  s_cfg.deadband_c = clampf(s_cfg.deadband_c, 0.05f, 1.0f);
  s_cfg.use_equitherm = s_cfg.use_equitherm ? 1 : 0;
  s_planRoomOffset = 0.0f;
  s_planStop = false;
  resetPid();
  s_u = 0.0f;
  s_uBase = 0.0f;
  s_lastPidMs = 0;
  s_histCount = 0;
  s_histHead = 0;
  s_wasAuto = false;
  ESP_LOGI(TAG, "init room_sp=%.1f ekviterm=%d curve %.0f@%.0f .. %.0f@%.0f",
           (double)s_cfg.room_sp_c, (int)s_cfg.use_equitherm,
           (double)s_cfg.u_cold_pct, (double)s_cfg.t_out_cold_c,
           (double)s_cfg.u_warm_pct, (double)s_cfg.t_out_warm_c);
}

void climateRegulatorSave(void) {
  storageSaveRegulatorConfig(&s_cfg);
}

const RegulatorConfig* climateRegulatorGetConfig(void) { return &s_cfg; }

RegulatorConfig* climateRegulatorGetConfigMutable(void) { return &s_cfg; }

void climateRegulatorAdjustRoomSp(float deltaC) {
  climateRegulatorSetRoomSp(s_cfg.room_sp_c + deltaC);
}

void climateRegulatorSetRoomSp(float c) {
  s_cfg.room_sp_c = clampf(c, 18.0f, 24.0f);
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
  out->u_pct = s_u;
  out->u_base_pct = s_uBase;
  out->u_trim_pct = s_uTrim;
  out->error_c = s_lastError;
  out->room_sp_c = climateRegulatorRoomSpEffective();
  out->room_c = climateBleIsOk() ? climateBleTempC() : UI_TEPLOTA_NEPLATNA;
  out->outdoor_c =
      climateBleOutdoorIsOk() ? climateBleOutdoorTempC() : UI_TEPLOTA_NEPLATNA;
  out->t_water_c = pctToWaterC(s_u);
  out->active = (uiEez.rezim == UI_REZIM_AUTO);
  out->room_ok = climateBleIsOk();
  out->outdoor_ok = climateBleOutdoorIsOk();
  out->use_equitherm = (s_cfg.use_equitherm != 0);
}

int climateRegulatorHistoryCount(void) { return s_histCount; }

void climateRegulatorHistoryGet(int index, RegulatorHistoryPoint* out) {
  if (!out || index < 0 || index >= s_histCount) {
    if (out) {
      memset(out, 0, sizeof(*out));
    }
    return;
  }
  const int start =
      (s_histCount < REG_HISTORY_LEN) ? 0 : s_histHead;
  const int i = (start + index) % REG_HISTORY_LEN;
  *out = s_hist[i];
}

void climateRegulatorTick(void) {
  const bool isAuto = (uiEez.rezim == UI_REZIM_AUTO);
  if (!isAuto) {
    if (s_wasAuto) {
      resetPid();
      s_haveWritten = false;
      ESP_LOGI(TAG, "leave Auto — PID reset");
    }
    s_wasAuto = false;
    return;
  }
  if (!s_wasAuto) {
    ESP_LOGI(TAG, "enter Auto");
    resetPid();
    s_haveWritten = false;
    s_lastPidMs = 0;
    s_u = 0.0f;
    s_uBase = 0.0f;
  }
  s_wasAuto = true;

  const uint32_t now = millis();
  if (s_lastPidMs != 0 && (now - s_lastPidMs) < kPidPeriodMs) {
    return;
  }
  s_lastPidMs = now;

  const float roomSp = climateRegulatorRoomSpEffective();
  const bool roomOk = climateBleIsOk();
  const bool outOk = climateBleOutdoorIsOk();
  const float roomC = roomOk ? climateBleTempC() : NAN;
  const float outC = outOk ? climateBleOutdoorTempC() : NAN;
  const bool ekv = (s_cfg.use_equitherm != 0);

  // Ekviterm base — v režimu PID only start 0 % (= min voda 20 °C)
  if (!ekv) {
    s_uBase = 0.0f;
  } else if (outOk && !isnan(outC)) {
    s_uBase = equithermPct(outC);
  } else {
    // bez venkovní T: drž poslední / fallback 0 %
    if (isnan(s_uBase)) {
      s_uBase = 0.0f;
    }
  }

  // PID only: trim ±100 % → celý rozsah 0–100 % (base 0 % = 20 °C)
  const float trimLim = ekv ? s_cfg.trim_limit_pct : 100.0f;
  const float iMax = trimLim / fmaxf(s_cfg.ki, 0.001f);

  float uTrim = 0.0f;
  float err = 0.0f;

  if (roomOk && !isnan(roomC)) {
    err = roomSp - roomC;  // >0 studeno (topit), <0 přetopeno (snižovat vodu)
    s_lastError = err;

    if (fabsf(err) > s_cfg.deadband_c) {
      // Integrál opačného znaménka brání korekci (typicky po náběhu topení) → shodit
      if (s_integral * err < 0.0f) {
        s_integral *= 0.15f;
        if (fabsf(s_integral) < 0.5f) {
          s_integral = 0.0f;
        }
      }

      const float dt = 1.0f;
      s_integral += err * dt;
      s_integral = clampf(s_integral, -iMax, iMax);

      float deriv = 0.0f;
      if (s_havePrev) {
        deriv = (err - s_prevError) / dt;
      }
      s_prevError = err;
      s_havePrev = true;

      // Při přetopení silnější P + rychle pryč s kladným biasem
      const float kpEff = (err < 0.0f) ? (s_cfg.kp * kCoolKpScale) : s_cfg.kp;
      if (err < 0.0f) {
        if (s_cfg.bias_pct > 0.0f) {
          s_cfg.bias_pct *= 0.5f;
        }
        s_cfg.bias_pct =
            clampf(s_cfg.bias_pct + kAdaptGain * err * 3.0f, -kAdaptLimit,
                   kAdaptLimit);
      } else {
        s_cfg.bias_pct =
            clampf(s_cfg.bias_pct + kAdaptGain * err, -kAdaptLimit, kAdaptLimit);
      }

      uTrim = kpEff * err + s_cfg.ki * s_integral + s_cfg.kd * deriv +
              s_cfg.bias_pct;
      uTrim = clampf(uTrim, -trimLim, trimLim);
    } else {
      // V pásmu: uvolnit I/bias směrem k 0, ať SP vody neklesá/nestoupá „navždy“
      s_integral *= 0.85f;
      s_cfg.bias_pct *= 0.97f;
      if (fabsf(s_integral) < 0.3f) {
        s_integral = 0.0f;
      }
      s_prevError = err;
      s_havePrev = true;
      uTrim = clampf(s_cfg.ki * s_integral + s_cfg.bias_pct, -trimLim, trimLim);
    }
  } else {
    s_lastError = 0.0f;
    uTrim = clampf(s_cfg.bias_pct, -trimLim, trimLim);
  }

  s_uTrim = uTrim;
  float uBaseEff = s_uBase;
  // Přetopení při vysoké ekvitermě (zima venku): stáhnout base, jinak trim ±25 % nestačí
  if (roomOk && err < -s_cfg.deadband_c) {
    const float pull = clampf(-err * 20.0f, 0.0f, 50.0f);
    uBaseEff = fmaxf(0.0f, s_uBase - pull);
  }
  s_u = clampf(uBaseEff + s_uTrim, 0.0f, 100.0f);
  const uint8_t tWater = pctToWaterC(s_u);

  ESP_LOGI(TAG,
           "tick ekv=%d room=%.1f sp=%.1f out=%.1f base=%.1f/%.1f trim=%.1f u=%.1f -> %uC I=%.1f bias=%.1f",
           (int)ekv, roomOk ? (double)roomC : -999.0, (double)roomSp,
           outOk ? (double)outC : -999.0, (double)s_uBase, (double)uBaseEff,
           (double)s_uTrim, (double)s_u, (unsigned)tWater, (double)s_integral,
           (double)s_cfg.bias_pct);

  if (!s_planStop) {
    maybeWriteSetpoint(tWater, now);
  }

  if (s_lastHistoryMs == 0 || (now - s_lastHistoryMs) >= kHistoryPeriodMs) {
    s_lastHistoryMs = now;
    pushHistory(roomOk ? roomC : UI_TEPLOTA_NEPLATNA, roomSp, s_u,
                outOk ? outC : UI_TEPLOTA_NEPLATNA);
  }
}
