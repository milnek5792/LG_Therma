#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*BridgeUartOutFn)(const char* line);

void bridgeOtaInit(BridgeUartOutFn uartOut);
void bridgeOtaTick(void);

bool bridgeOtaWifiBusy(void);
bool bridgeOtaStartWifi(const char* ssid, const char* pass);
bool bridgeOtaStartWifiStored(void);
void bridgeOtaStopWifi(void);

#ifdef __cplusplus
}
#endif
