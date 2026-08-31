#ifndef NET_WIFI_MGR_H
#define NET_WIFI_MGR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void netWifiInit();
void netWifiTick();
void netWifiSetEnabled(bool on);
bool netWifiIsEnabled();
void netWifiConnect();
bool netWifiIsConnected();
bool netWifiHasCredentials();
/** Aktuální SSID/heslo (NVS + runtime). */
bool netWifiCopyCredentials(char* ssid, size_t ssidLen, char* pass, size_t passLen);
void netWifiSetCredentials(const char* ssid, const char* pass);
const char* netWifiStatus();
const char* netWifiSsid();
const char* netWifiIp();

bool netWifiIsBusy(void);

/** Legacy no-op — BLE je na externím H2, Wi‑Fi se nesuspenduje. */
bool netWifiIsSuspendedForBle(void);
bool netWifiSuspendForBle(void);
bool netWifiResumeAfterBle(void);

#ifdef __cplusplus
}
#endif

#endif
