// climate_room.cpp — NimBLE backend (7B). Tab5: climate_room_uart.cpp
#include "lg_board.h"

#if LG_HAS_NATIVE_BLE

#include "climate_room.h"
#include "climate_ble_room.h"
#include <stdio.h>

void climateRoomInit(void) { climateBleInit(); }
void climateRoomTick(void) { climateBleTick(); }
void climateRoomRequestNow(void) { climateBleRequestNow(); }

bool climateRoomIsOk(void) { return climateBleIsOk(); }
bool climateRoomIsBusy(void) { return climateBleIsBusy(); }
void climateRoomReleaseForTls(void) { climateBleReleaseForTls(); }
bool climateRoomBootPollPending(void) { return climateBleBootPollPending(); }

float climateRoomTempC(void) { return climateBleTempC(); }
float climateRoomHumidity(void) { return climateBleHumidity(); }
int climateRoomBatteryPct(void) { return climateBleBatteryPct(); }
int climateRoomRssi(void) { return climateBleRssi(); }

bool climateRoomOutdoorIsOk(void) { return climateBleOutdoorIsOk(); }
float climateRoomOutdoorTempC(void) { return climateBleOutdoorTempC(); }
float climateRoomOutdoorHumidity(void) { return climateBleOutdoorHumidity(); }
int climateRoomOutdoorBatteryPct(void) { return climateBleOutdoorBatteryPct(); }
int climateRoomOutdoorRssi(void) { return climateBleOutdoorRssi(); }

void climateRoomStatusText(char* buf, size_t buflen) {
  climateBleStatusText(buf, buflen);
}

#endif
