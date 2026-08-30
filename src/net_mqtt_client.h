#ifndef NET_MQTT_CLIENT_H
#define NET_MQTT_CLIENT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void netMqttInit();
/** Zavolat co nejdřív po M5.begin() (před LVGL) — rezervuje INTERNAL pro TLS. */
void netMqttReserveTlsMemory();
void netMqttTick();
void netMqttSetEnabled(bool on);
bool netMqttIsEnabled();
void netMqttConnect();
bool netMqttIsConnected();
/** true po prvním úspěšném MQTT connectu. */
bool netMqttBootSettled(void);
bool netMqttIsBusy(void);
bool netMqttIsWatchActive(void);
const char* netMqttStatus();
const char* netMqttHost();

#ifdef __cplusplus
}
#endif

#endif
