// net_sdio_arbiter.h — lehké řízení UI vs MQTT (bez BLE)
//
// BLE je na externím ESP32-H2 (UART). Tab5 C6 jen Wi‑Fi/MQTT.
// Arbiter drží: watch okno, TLS busy (UI light/freeze), UI heavy.
#ifndef NET_SDIO_ARBITER_H
#define NET_SDIO_ARBITER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NET_SDIO_NONE = 0,
  NET_SDIO_MQTT,
  NET_SDIO_UI,
} NetSdioOwner;

typedef enum {
  NET_SDIO_PRESSURE_OK = 0,
  NET_SDIO_PRESSURE_LOW,
  NET_SDIO_PRESSURE_CRITICAL,
} NetSdioPressure;

void netSdioInit(void);
void netSdioTick(void);

NetSdioOwner netSdioOwner(void);
NetSdioPressure netSdioPressure(void);
size_t netSdioDmaMax(void);

/** true = TLS/UI heavy — displej radši CPU blit. */
bool netSdioRadioBusy(void);

bool netSdioMqttSession(void);
bool netSdioMqttWanted(void);
bool netSdioCanMqtt(void);
bool netSdioTryBeginMqtt(void);
void netSdioEndMqtt(void);

void netSdioBumpWatch(uint32_t holdMs);
void netSdioBumpWatchDefault(void);
void netSdioWatchOff(void);

void netSdioSetMqttSession(bool online);
void netSdioSetTlsBusy(bool busy);
bool netSdioTlsBusy(void);

void netSdioHoldUiFreeze(uint32_t holdMs);
void netSdioClearUiFreeze(void);

void netSdioBeginUiHeavy(uint32_t holdMs);
void netSdioBeginUiHeavyDefault(void);
void netSdioEndUiHeavy(void);

#ifdef __cplusplus
}
#endif

#endif
