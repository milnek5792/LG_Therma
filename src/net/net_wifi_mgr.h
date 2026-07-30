#ifndef NET_WIFI_MGR_H
#define NET_WIFI_MGR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void netWifiInit();
void netWifiTick();
void netWifiSetEnabled(bool on);
bool netWifiIsEnabled();
void netWifiConnect();
bool netWifiIsConnected();
/** true při arm/connecting — UI omezí těžké překreslení. */
bool netWifiIsBusy(void);
bool netWifiHasCredentials();
void netWifiSetCredentials(const char* ssid, const char* pass);
const char* netWifiStatus();
const char* netWifiSsid();
const char* netWifiIp();

/** Legacy no-op — na 7B není SDIO/C6 BLE konflikt. */
bool netWifiIsSuspendedForBle(void);
bool netWifiSuspendForBle(void);
bool netWifiResumeAfterBle(void);

#ifdef __cplusplus
}
#endif

#endif
