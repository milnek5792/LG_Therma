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

void climateRoomStatusText(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif
