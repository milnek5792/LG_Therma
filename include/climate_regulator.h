// climate_regulator.h — adaptivní ekviterm + PI korekce + asym. Eco
#ifndef CLIMATE_REGULATOR_H
#define CLIMATE_REGULATOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REG_HISTORY_LEN 60

/** Výstupní SP vody (LIN) — hard safety. */
#define REG_T_WATER_MIN_C 20
#define REG_T_WATER_MAX_C 45

/** PI korekce k ekvitermnímu základu [°C]. */
#define REG_CORR_MIN_C (-5.0f)
#define REG_CORR_MAX_C (3.0f)

/** Asymetrické Eco: vstup při SP+hyst, návrat při ≤ SP. */
#define REG_ECO_HYST_C (0.3f)

struct RegulatorConfig {
  float room_sp_c;       // default 22
  float t_out_cold_c;    // NVS legacy — křivka teď pevná -18/45
  float u_cold_pct;      // NVS legacy
  float t_out_warm_c;    // NVS legacy
  float u_warm_pct;      // NVS legacy
  /** Kp: °C korekce / °C pokoje (default 4). */
  float kp;
  /** Ki: °C korekce / (°C · perioda 120 s) (default 0.1). */
  float ki;
  /** Kd: obvykle 0. */
  float kd;
  float bias_pct;        // NVS legacy — nepoužito
  float trim_limit_pct;  // legacy
  float deadband_c;      // legacy
  /** 1 = ekvitermní základ; 0 = fixní střed 32.5 °C + PI. */
  uint8_t use_equitherm;
  uint8_t _pad[3];
};

struct RegulatorSnapshot {
  float t_water_sp_c;   // výstup po clamp [°C]
  float t_water_raw_c;  // před clampem [°C]
  float eq_base_c;      // ekviterm / fallback základ
  float pid_corr_c;     // PI korekce (−5…+3)
  float p_term_c;       // Kp·e
  float i_term_c;       // integrál (zmražen v Eco)
  float error_c;
  float room_sp_c;
  float room_c;
  float outdoor_c;
  uint8_t t_water_c;    // zaokrouhleno pro LIN
  bool active;
  bool eco_mode;
  bool room_ok;
  bool outdoor_ok;
  bool use_equitherm;
  uint32_t ms_since_pid;
  uint32_t ms_to_next_pid;
  uint32_t pid_period_ms;
};

struct RegulatorHistoryPoint {
  float room_c;
  float room_sp_c;
  float t_water_c;
  float outdoor_c;
};

void climateRegulatorInit(void);
void climateRegulatorTick(void);
void climateRegulatorRequestImmediateTick(void);
void climateRegulatorSave(void);
/** Odložený zápis po změně pokojového SP (HMI/MQTT) — flush v uiBusFlushDeferredStorage. */
void climateRegulatorRequestSave(void);
void climateRegulatorFlushPendingSave(void);

const RegulatorConfig* climateRegulatorGetConfig(void);
RegulatorConfig* climateRegulatorGetConfigMutable(void);
void climateRegulatorSetDefaults(RegulatorConfig* cfg);
void climateRegulatorSetUseEquitherm(bool on);
bool climateRegulatorUseEquitherm(void);
bool climateRegulatorIsEcoMode(void);

void climateRegulatorGetSnapshot(RegulatorSnapshot* out);
void climateRegulatorAdjustRoomSp(float deltaC);
void climateRegulatorSetRoomSp(float c);
float climateRegulatorRoomSpEffective(void);

void climateRegulatorSetPlanRoomOffset(float offsetC);
float climateRegulatorPlanRoomOffset(void);
bool climateRegulatorPlanRequestsStop(void);
void climateRegulatorSetPlanStop(bool stop);

int climateRegulatorHistoryCount(void);
uint32_t climateRegulatorHistoryGen(void);
void climateRegulatorHistoryGet(int index, RegulatorHistoryPoint* out);

#ifdef __cplusplus
}
#endif

#endif
