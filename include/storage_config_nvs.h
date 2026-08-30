#ifndef STORAGE_CONFIG_NVS_H
#define STORAGE_CONFIG_NVS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "climate_plan.h"
#include "climate_regulator.h"

#ifdef __cplusplus
extern "C" {
#endif

void storageInit();
bool storageLoadWifiEnabled();
/** true, pokud je klíč wifi_en už v NVS (explicitní volba uživatele / dřívější save). */
bool storageWifiEnabledIsSet();
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

bool storageLoadRegulatorConfig(RegulatorConfig* cfg);
void storageSaveRegulatorConfig(const RegulatorConfig* cfg);

/** uiEez.rezim: 0 = auto, 1 = vystupni teplota (UiRezimRegulace). */
bool storageLoadUiRezim(uint8_t* out);
void storageSaveUiRezim(uint8_t rezim);

/** Poslední HMI session TČ (přežije FW restart). outSp volitelné. */
bool storageLoadTcSession(bool* outOn, uint8_t* outSp);
void storageSaveTcSession(bool on, uint8_t spC);
/** Odložený zápis — nevolat z LIN tasku (flash blokuje UART). */
void storageRequestSaveTcSession(bool on, uint8_t spC);
void storageFlushTcSessionPending(void);

/** SwitchBot MAC pro H2 bridge (pokoj / venku). Prázdný = neuloženo. */
bool storageLoadBleRoomMac(char* mac, size_t len);
void storageSaveBleRoomMac(const char* mac);
bool storageLoadBleOutdoorMac(char* mac, size_t len);
void storageSaveBleOutdoorMac(const char* mac);

#ifdef __cplusplus
}
#endif

#endif
