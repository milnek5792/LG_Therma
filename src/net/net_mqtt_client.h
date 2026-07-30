#ifndef NET_MQTT_CLIENT_H
#define NET_MQTT_CLIENT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void netMqttInit(void);
void netMqttTick(void);
void netMqttSetEnabled(bool on);
bool netMqttIsEnabled(void);
void netMqttConnect(void);
bool netMqttIsConnected(void);
bool netMqttIsBusy(void);
/** Odpoj MQTT před WIFI_OFF / BLE (bez smazání wantConnect). */
void netMqttDisconnectQuiet(void);
const char* netMqttStatus(void);
const char* netMqttHost(void);

#ifdef __cplusplus
}
#endif

#endif
