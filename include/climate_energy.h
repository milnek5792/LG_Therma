// climate_energy.h — spotřeba TČ z PZEM (ΔEnergy) + historie
#ifndef CLIMATE_ENERGY_H
#define CLIMATE_ENERGY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENERGY_MINUTES_PER_DAY 1440
#define ENERGY_WEEK_DAYS 7
#define ENERGY_SEASON_MONTHS 9
#define ENERGY_YEAR_SLOTS 5

/** Index měsíce v sezóně: 0=Zář … 8=Kvě. -1 = mimo topné období. */
int climateEnergySeasonMonthIndex(int calendarMonth1to12);

/** Rok startu sezóny (září Y … květen Y+1 → Y). */
int climateEnergySeasonYear(int calendarYear, int calendarMonth1to12);

void climateEnergyInit(void);
void climateEnergyTick(void);

/** Vzorek z bridge UART: W, E[kWh], R=reset flag. */
void climateEnergyOnSample(uint16_t avgPowerW, float energyKwh, bool energyReset);

bool climateEnergyIsOk(void);
uint16_t climateEnergyPowerW(void);
float climateEnergyTodayKwh(void);
float climateEnergyMonthKwh(void);
float climateEnergyYearKwh(void);

/** dayOffset: 0 = dnes, 1 = včera, … max 6. */
bool climateEnergyDayPowerGet(int dayOffset, const uint16_t** outSamples,
                              float* outDayKwh, int* outYmd);
uint32_t climateEnergyHistoryGen(void);

/** Měsíce aktuální sezóny [0..8] = Zář..Kvě. */
float climateEnergySeasonMonthKwh(int seasonMonthIndex);
int climateEnergyCurrentSeasonYear(void);

/** Roky: index 0 = nejnovější. */
bool climateEnergyYearGet(int index, int* outYear, float* outKwh);

#ifdef __cplusplus
}
#endif

#endif
