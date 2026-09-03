// climate_room_uart.h — pokojová T z ESP32-H2 přes UART (M5-Bus G6/G7)
#ifndef CLIMATE_ROOM_UART_H
#define CLIMATE_ROOM_UART_H

#include "h2_uart_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLIMATE_ROOM_UART_RX_PIN
#define CLIMATE_ROOM_UART_RX_PIN 7
#endif
#ifndef CLIMATE_ROOM_UART_TX_PIN
#define CLIMATE_ROOM_UART_TX_PIN 6
#endif
#ifndef CLIMATE_ROOM_UART_BAUD
#define CLIMATE_ROOM_UART_BAUD 115200
#endif

typedef struct {
  char mac[H2_MAC_STR_LEN];
  float temp_c;
  int rssi;
  bool valid;
} ClimateRoomFound;

void climateRoomInit(void);
void climateRoomTick(void);
void climateRoomRequestNow(void);
void climateRoomStartScan(void);
bool climateRoomSelectMeter(uint8_t index1);
bool climateRoomSetRoomMac(const char* mac);
bool climateRoomSetOutdoorMac(const char* mac);
bool climateRoomPushConfig(void);

bool climateRoomIsOk(void);
bool climateRoomIsBusy(void);
void climateRoomReleaseForTls(void);
bool climateRoomBootPollPending(void);

int climateRoomFoundCount(void);
bool climateRoomGetFound(uint8_t index1, ClimateRoomFound* out);
void climateRoomGetConfiguredMac(char* buf, size_t len);
void climateRoomGetConfiguredOutdoorMac(char* buf, size_t len);

float climateRoomTempC(void);
float climateRoomHumidity(void);
int climateRoomBatteryPct(void);
int climateRoomRssi(void);

bool climateRoomOutdoorIsOk(void);
float climateRoomOutdoorTempC(void);
float climateRoomOutdoorHumidity(void);
int climateRoomOutdoorBatteryPct(void);
int climateRoomOutdoorRssi(void);

void climateRoomGetLastRoomResponse(char* buf, size_t len);
void climateRoomGetLastOutdoorResponse(char* buf, size_t len);

/** OTA bridge: Tab5 pošle Wi‑Fi creds přes UART, bridge spustí ArduinoOTA. */
typedef enum {
  CLIMATE_BRIDGE_OTA_IDLE = 0,
  CLIMATE_BRIDGE_OTA_CONNECTING,
  CLIMATE_BRIDGE_OTA_READY,
  CLIMATE_BRIDGE_OTA_FAIL,
} ClimateBridgeOtaState;

void climateRoomBridgeOtaStart(void);
bool climateRoomBridgeOtaStartWith(const char* ssid, const char* pass);
void climateRoomBridgeOtaStop(void);
ClimateBridgeOtaState climateRoomBridgeOtaState(void);
const char* climateRoomBridgeOtaIp(void);
const char* climateRoomBridgeOtaHost(void);

/** Diagnostika bridge: `GET INFO` → `INFO MAC=… CH=… ESPNOW=…` */
void climateRoomRequestBridgeInfo(void);
bool climateRoomBridgeInfoOk(void);
const char* climateRoomBridgeMac(void);
uint8_t climateRoomBridgeChannel(void);
bool climateRoomBridgeEspNowOk(void);
uint32_t climateRoomBridgeInfoAgeMs(void);

/** Poslední PWR vzorek z UART (pro diagnostiku). */
bool climateRoomLastPwrOk(void);
uint16_t climateRoomLastPwrW(void);
float climateRoomLastPwrKwh(void);
uint32_t climateRoomLastPwrAgeMs(void);

void climateRoomStatusText(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif
