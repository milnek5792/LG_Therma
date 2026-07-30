#ifndef NET_SDIO_ARBITER_H
#define NET_SDIO_ARBITER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void netSdioInit(void);
void netSdioTick(void);
bool netSdioCanBle(void);
bool netSdioTryBeginBle(void);
void netSdioEndBle(void);
bool netSdioMqttWanted(void);
bool netSdioCanMqtt(void);
void netSdioBumpWatch(uint32_t holdMs);
void netSdioBumpWatchDefault(void);
void netSdioWatchOff(void);
void netSdioSetMqttSession(bool online);
void netSdioBeginUiHeavy(uint32_t holdMs);
void netSdioBeginUiHeavyDefault(void);
bool netSdioTlsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
