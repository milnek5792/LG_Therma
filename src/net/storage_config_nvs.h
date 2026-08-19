#ifndef STORAGE_CONFIG_NVS_H
#define STORAGE_CONFIG_NVS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "climate_plan.h"

#ifdef __cplusplus
extern "C" {
#endif

void storageInit();
bool storageLoadWifiEnabled();
void storageSaveWifiEnabled(bool on);
bool storageLoadWifiCredentials(char* ssid, size_t ssidLen, char* pass, size_t passLen);
void storageSaveWifiCredentials(const char* ssid, const char* pass);
bool storageLoadMqttEnabled();
void storageSaveMqttEnabled(bool on);

/** Jas displeje 10–97 %, default 60. */
uint8_t storageLoadBrightness(void);
void storageSaveBrightness(uint8_t percent);
/** Usínání: 0 = vypnuto, jinak sekundy. Default 120. */
uint32_t storageLoadSleepTimeoutSec(void);
void storageSaveSleepTimeoutSec(uint32_t sec);

bool storageLoadPlanConfig(PlanTydenConfig* cfg);
void storageSavePlanConfig(const PlanTydenConfig* cfg);

#ifdef __cplusplus
}
#endif

#endif
