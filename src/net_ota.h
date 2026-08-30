#ifndef NET_OTA_H
#define NET_OTA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ArduinoOTA — upload z PlatformIO (`upload_protocol = espota`). */
void netOtaInit(void);
void netOtaTick(void);
bool netOtaIsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
