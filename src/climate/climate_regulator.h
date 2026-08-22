// climate_regulator.h — ekviterm + adaptivní PID trim (akční veličina 0–100 %)
#ifndef CLIMATE_REGULATOR_H
#define CLIMATE_REGULATOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REG_HISTORY_LEN 60

/** Mapování: 0 % → 20 °C, 100 % → 45 °C */
#define REG_T_WATER_MIN_C 20
#define REG_T_WATER_MAX_C 45

struct RegulatorConfig {
  float room_sp_c;       // default 22
  float t_out_cold_c;    // -15 → 100 %
  float u_cold_pct;      // 100
  float t_out_warm_c;    // 15 → 0 %
  float u_warm_pct;      // 0
  float kp;
  float ki;
  float kd;
  float bias_pct;        // adaptivní bias
  float trim_limit_pct;  // ±25 (ekviterm+PID)
  float deadband_c;      // 0.2
  /** 1 = ekviterm + PID trim; 0 = jen PID (bez venkovního teploměru). */
  uint8_t use_equitherm;
  uint8_t _pad[3];
};

struct RegulatorSnapshot {
  float u_pct;
  float u_base_pct;
  float u_trim_pct;
  float error_c;
  float room_sp_c;
  float room_c;
  float outdoor_c;
  uint8_t t_water_c;
  bool active;
  bool room_ok;
  bool outdoor_ok;
  bool use_equitherm;
};

struct RegulatorHistoryPoint {
  float room_c;
  float room_sp_c;
  float u_pct;
  float outdoor_c;
};

void climateRegulatorInit(void);
void climateRegulatorTick(void);
void climateRegulatorSave(void);

const RegulatorConfig* climateRegulatorGetConfig(void);
RegulatorConfig* climateRegulatorGetConfigMutable(void);
void climateRegulatorSetDefaults(RegulatorConfig* cfg);
void climateRegulatorSetUseEquitherm(bool on);
bool climateRegulatorUseEquitherm(void);

void climateRegulatorGetSnapshot(RegulatorSnapshot* out);
void climateRegulatorAdjustRoomSp(float deltaC);
void climateRegulatorSetRoomSp(float c);
float climateRegulatorRoomSpEffective(void);

/** Plán: offset pokojového SP (UTLUM), 0 = bez offsetu. */
void climateRegulatorSetPlanRoomOffset(float offsetC);
float climateRegulatorPlanRoomOffset(void);
bool climateRegulatorPlanRequestsStop(void);
void climateRegulatorSetPlanStop(bool stop);

/** Historie pro graf (nejstarší → nejnovější). */
int climateRegulatorHistoryCount(void);
void climateRegulatorHistoryGet(int index, RegulatorHistoryPoint* out);

float climateRegulatorPctToTemp(float uPct);
float climateRegulatorTempToPct(float tC);

#ifdef __cplusplus
}
#endif

#endif
