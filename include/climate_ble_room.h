// climate_ble_room.h — SwitchBot Meter(y) přes NimBLE (scan 2 min, Wi‑Fi běží dál)
#ifndef CLIMATE_BLE_ROOM_H
#define CLIMATE_BLE_ROOM_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void climateBleInit(void);
void climateBleTick(void);
/** Okamžitý poll (tlačítko Nastaveni). */
void climateBleRequestNow(void);

bool climateBleIsOk(void);
bool climateBleIsBusy(void);
/** Uvolni NimBLE stack před TLS (šetří ~30 KB interní DRAM). */
void climateBleReleaseForTls(void);
/** První scan po bootu ještě neproběhl — MQTT má počkat. */
bool climateBleBootPollPending(void);
float climateBleTempC(void);
float climateBleHumidity(void);
int climateBleBatteryPct(void);
int climateBleRssi(void);

bool climateBleOutdoorIsOk(void);
float climateBleOutdoorTempC(void);
float climateBleOutdoorHumidity(void);
int climateBleOutdoorBatteryPct(void);
int climateBleOutdoorRssi(void);

/** Stavový řádek pro UI (Nastaveni) — pokoj + venkovní. */
void climateBleStatusText(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif
