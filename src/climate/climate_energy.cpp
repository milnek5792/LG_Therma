// climate_energy.cpp — ΔEnergy z PZEM + týdenní/sezónní/roční součty
#include "climate_energy.h"

#include "storage_config_nvs.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <time.h>

namespace {

constexpr uint32_t kSaveIntervalMs = 30UL * 60UL * 1000UL;       // meta ~30 min
constexpr uint32_t kPowerSaveIntervalMs = 30UL * 60UL * 1000UL;  // příkon ~30 min
constexpr float kWhEps = 0.0005f;

struct EnergyMeta {
  uint32_t magic;
  uint16_t version;
  uint16_t _pad;
  uint32_t e_prev_wh;
  uint8_t have_prev;
  uint8_t _pad2[3];
  int32_t day_ymd[ENERGY_WEEK_DAYS];
  float day_kwh[ENERGY_WEEK_DAYS];
  int32_t season_year;
  float season_month_kwh[ENERGY_SEASON_MONTHS];
  int32_t year_id[ENERGY_YEAR_SLOTS];
  float year_kwh[ENERGY_YEAR_SLOTS];
  int32_t last_ymd;
  int32_t last_month;
  int32_t last_cal_year;
};

constexpr uint32_t kMetaMagic = 0x454E5231u;  // ENR1
constexpr uint16_t kMetaVersion = 1;

uint16_t* s_weekPower = nullptr;  // 7 * 1440
EnergyMeta s_meta{};
bool s_ok = false;
uint16_t s_powerW = 0;
uint32_t s_histGen = 1;
uint32_t s_lastSaveMs = 0;
uint32_t s_lastPowerSaveMs = 0;
bool s_metaDirty = false;
bool s_powerDirty = false;
bool s_weekFullDirty = false;

int ymdFromTm(const struct tm& t) {
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

bool localNow(struct tm* out) {
  if (!out) {
    return false;
  }
  time_t now = time(nullptr);
  if (now < 1700000000) {  // ~2023 — NTP ještě ne
    return false;
  }
  return localtime_r(&now, out) != nullptr;
}

void markMetaDirty() {
  s_metaDirty = true;
}

void ensureYearSlot(int year) {
  if (s_meta.year_id[0] == year) {
    return;
  }
  // Najdi existující
  for (int i = 0; i < ENERGY_YEAR_SLOTS; ++i) {
    if (s_meta.year_id[i] == year) {
      const int id = s_meta.year_id[i];
      const float kwh = s_meta.year_kwh[i];
      memmove(&s_meta.year_id[1], &s_meta.year_id[0],
              (size_t)i * sizeof(int32_t));
      memmove(&s_meta.year_kwh[1], &s_meta.year_kwh[0],
              (size_t)i * sizeof(float));
      s_meta.year_id[0] = id;
      s_meta.year_kwh[0] = kwh;
      markMetaDirty();
      return;
    }
  }
  // Nový rok — posuň historii
  memmove(&s_meta.year_id[1], &s_meta.year_id[0],
          (ENERGY_YEAR_SLOTS - 1) * sizeof(int32_t));
  memmove(&s_meta.year_kwh[1], &s_meta.year_kwh[0],
          (ENERGY_YEAR_SLOTS - 1) * sizeof(float));
  s_meta.year_id[0] = year;
  s_meta.year_kwh[0] = 0.0f;
  markMetaDirty();
}

void ensureSeason(int seasonYear) {
  if (s_meta.season_year == seasonYear) {
    return;
  }
  s_meta.season_year = seasonYear;
  for (int i = 0; i < ENERGY_SEASON_MONTHS; ++i) {
    s_meta.season_month_kwh[i] = 0.0f;
  }
  markMetaDirty();
}

void rollDay(int newYmd) {
  // Posuň týdenní buffer o 1 den (index 0 = dnes)
  memmove(&s_weekPower[ENERGY_MINUTES_PER_DAY], &s_weekPower[0],
          (ENERGY_WEEK_DAYS - 1) * ENERGY_MINUTES_PER_DAY * sizeof(uint16_t));
  memset(&s_weekPower[0], 0, ENERGY_MINUTES_PER_DAY * sizeof(uint16_t));

  for (int i = ENERGY_WEEK_DAYS - 1; i > 0; --i) {
    s_meta.day_ymd[i] = s_meta.day_ymd[i - 1];
    s_meta.day_kwh[i] = s_meta.day_kwh[i - 1];
  }
  s_meta.day_ymd[0] = newYmd;
  s_meta.day_kwh[0] = 0.0f;
  s_meta.last_ymd = newYmd;
  s_powerDirty = true;
  s_weekFullDirty = true;  // posunuté dny zapsat najednou (mimo minutový PWR)
  markMetaDirty();
  s_histGen++;
}

void applyDelta(float deltaKwh, const struct tm& t) {
  if (deltaKwh < 0.0f) {
    deltaKwh = 0.0f;
  }
  if (deltaKwh < kWhEps) {
    return;
  }

  const int ymd = ymdFromTm(t);
  const int month = t.tm_mon + 1;
  const int year = t.tm_year + 1900;
  const int seasonYear = climateEnergySeasonYear(year, month);
  const int seasonIdx = climateEnergySeasonMonthIndex(month);

  if (s_meta.last_ymd != 0 && ymd != s_meta.last_ymd) {
    // Midnight / NTP catch-up — roll missing days at most 6
    int guard = 0;
    while (s_meta.last_ymd != ymd && guard < ENERGY_WEEK_DAYS) {
      rollDay(ymd);  // simplified: jump to today
      break;
    }
  }
  if (s_meta.day_ymd[0] != ymd) {
    if (s_meta.day_ymd[0] == 0) {
      s_meta.day_ymd[0] = ymd;
      s_meta.last_ymd = ymd;
    } else {
      rollDay(ymd);
    }
  }

  ensureYearSlot(year);
  ensureSeason(seasonYear);

  s_meta.day_kwh[0] += deltaKwh;
  s_meta.year_kwh[0] += deltaKwh;
  if (seasonIdx >= 0) {
    s_meta.season_month_kwh[seasonIdx] += deltaKwh;
  }
  s_meta.last_month = month;
  s_meta.last_cal_year = year;
  markMetaDirty();
  s_histGen++;
}

void persistIfNeeded(bool force) {
  const uint32_t now = millis();
  const bool metaDue = force || ((now - s_lastSaveMs) >= kSaveIntervalMs);
  const bool powerDue =
      force || s_weekFullDirty ||
      ((now - s_lastPowerSaveMs) >= kPowerSaveIntervalMs);

  if (s_metaDirty && metaDue) {
    storageSaveEnergyMeta(&s_meta, sizeof(s_meta));
    s_metaDirty = false;
    s_lastSaveMs = now;
  }

  if (s_powerDirty && s_weekPower && powerDue) {
    if (s_weekFullDirty || force) {
      storageSaveEnergyWeekPower(s_weekPower,
                                 ENERGY_WEEK_DAYS * ENERGY_MINUTES_PER_DAY);
      s_weekFullDirty = false;
    } else {
      // Běžný minutový vzorek: jen dnešek — 7× putBytes shazovalo panel (~1×/min).
      storageSaveEnergyWeekPowerDay(0, s_weekPower);
    }
    s_powerDirty = false;
    s_lastPowerSaveMs = now;
  }
}

}  // namespace

int climateEnergySeasonMonthIndex(int calendarMonth1to12) {
  // Zář=9 → 0 … Pro=12 → 3, Led=1 → 4 … Kvě=5 → 8
  if (calendarMonth1to12 >= 9 && calendarMonth1to12 <= 12) {
    return calendarMonth1to12 - 9;
  }
  if (calendarMonth1to12 >= 1 && calendarMonth1to12 <= 5) {
    return calendarMonth1to12 + 3;
  }
  return -1;
}

int climateEnergySeasonYear(int calendarYear, int calendarMonth1to12) {
  if (calendarMonth1to12 >= 1 && calendarMonth1to12 <= 5) {
    return calendarYear - 1;
  }
  return calendarYear;
}

void climateEnergyInit(void) {
  if (!s_weekPower) {
    s_weekPower = (uint16_t*)ps_calloc(ENERGY_WEEK_DAYS * ENERGY_MINUTES_PER_DAY,
                                       sizeof(uint16_t));
    if (!s_weekPower) {
      s_weekPower = (uint16_t*)calloc(ENERGY_WEEK_DAYS * ENERGY_MINUTES_PER_DAY,
                                      sizeof(uint16_t));
    }
  }
  memset(&s_meta, 0, sizeof(s_meta));
  s_meta.magic = kMetaMagic;
  s_meta.version = kMetaVersion;

  EnergyMeta loaded{};
  if (storageLoadEnergyMeta(&loaded, sizeof(loaded)) &&
      loaded.magic == kMetaMagic && loaded.version == kMetaVersion) {
    s_meta = loaded;
  }
  if (s_weekPower) {
    storageLoadEnergyWeekPower(s_weekPower,
                               ENERGY_WEEK_DAYS * ENERGY_MINUTES_PER_DAY);
  }
  Serial.printf("[ENERGY] init e_prev=%lu Wh season=%ld\n",
                (unsigned long)s_meta.e_prev_wh, (long)s_meta.season_year);
}

void climateEnergyTick(void) {
  struct tm t{};
  if (localNow(&t)) {
    const int ymd = ymdFromTm(t);
    if (s_meta.last_ymd != 0 && ymd != s_meta.last_ymd) {
      rollDay(ymd);
    } else if (s_meta.day_ymd[0] == 0) {
      s_meta.day_ymd[0] = ymd;
      s_meta.last_ymd = ymd;
      markMetaDirty();
    }
    ensureYearSlot(t.tm_year + 1900);
    ensureSeason(climateEnergySeasonYear(t.tm_year + 1900, t.tm_mon + 1));
  }
  persistIfNeeded(false);
}

void climateEnergyOnSample(uint16_t avgPowerW, float energyKwh, bool energyReset) {
  s_powerW = avgPowerW;
  s_ok = true;

  struct tm t{};
  const bool haveTime = localNow(&t);
  const int minuteOfDay =
      haveTime ? (t.tm_hour * 60 + t.tm_min) : -1;

  if (s_weekPower && minuteOfDay >= 0 && minuteOfDay < ENERGY_MINUTES_PER_DAY) {
    if (s_meta.day_ymd[0] == 0 && haveTime) {
      s_meta.day_ymd[0] = ymdFromTm(t);
      s_meta.last_ymd = s_meta.day_ymd[0];
    }
    s_weekPower[minuteOfDay] = avgPowerW;
    s_powerDirty = true;
    s_histGen++;
  }

  uint32_t energyWh = 0;
  if (energyKwh > 0.0f) {
    energyWh = (uint32_t)(energyKwh * 1000.0f + 0.5f);
  }

  float deltaKwh = 0.0f;
  if (!s_meta.have_prev) {
    s_meta.e_prev_wh = energyWh;
    s_meta.have_prev = 1;
    markMetaDirty();
  } else if (energyReset || energyWh < s_meta.e_prev_wh) {
    // Po resetu: nová hodnota = spotřeba od nuly
    deltaKwh = (float)energyWh / 1000.0f;
    s_meta.e_prev_wh = energyWh;
    markMetaDirty();
  } else if (energyWh != s_meta.e_prev_wh) {
    deltaKwh = (float)(energyWh - s_meta.e_prev_wh) / 1000.0f;
    s_meta.e_prev_wh = energyWh;
    markMetaDirty();
  }

  if (haveTime && deltaKwh > kWhEps) {
    applyDelta(deltaKwh, t);
  }

  // Flash zápis ne tady (UART RX / UI) — climateEnergyTick()
}

bool climateEnergyIsOk(void) { return s_ok; }

uint16_t climateEnergyPowerW(void) { return s_powerW; }

float climateEnergyTodayKwh(void) { return s_meta.day_kwh[0]; }

float climateEnergyMonthKwh(void) {
  struct tm t{};
  if (!localNow(&t)) {
    return 0.0f;
  }
  const int idx = climateEnergySeasonMonthIndex(t.tm_mon + 1);
  if (idx < 0) {
    return 0.0f;
  }
  return s_meta.season_month_kwh[idx];
}

float climateEnergyYearKwh(void) { return s_meta.year_kwh[0]; }

bool climateEnergyDayPowerGet(int dayOffset, const uint16_t** outSamples,
                              float* outDayKwh, int* outYmd) {
  if (dayOffset < 0 || dayOffset >= ENERGY_WEEK_DAYS || !s_weekPower) {
    return false;
  }
  if (outSamples) {
    *outSamples = &s_weekPower[dayOffset * ENERGY_MINUTES_PER_DAY];
  }
  if (outDayKwh) {
    *outDayKwh = s_meta.day_kwh[dayOffset];
  }
  if (outYmd) {
    *outYmd = (int)s_meta.day_ymd[dayOffset];
  }
  return true;
}

uint32_t climateEnergyHistoryGen(void) { return s_histGen; }

float climateEnergySeasonMonthKwh(int seasonMonthIndex) {
  if (seasonMonthIndex < 0 || seasonMonthIndex >= ENERGY_SEASON_MONTHS) {
    return 0.0f;
  }
  return s_meta.season_month_kwh[seasonMonthIndex];
}

int climateEnergyCurrentSeasonYear(void) { return (int)s_meta.season_year; }

bool climateEnergyYearGet(int index, int* outYear, float* outKwh) {
  if (index < 0 || index >= ENERGY_YEAR_SLOTS) {
    return false;
  }
  if (s_meta.year_id[index] == 0) {
    return false;
  }
  if (outYear) {
    *outYear = (int)s_meta.year_id[index];
  }
  if (outKwh) {
    *outKwh = s_meta.year_kwh[index];
  }
  return true;
}
