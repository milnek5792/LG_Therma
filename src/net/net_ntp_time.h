#ifndef NET_NTP_TIME_H
#define NET_NTP_TIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void netNtpInit(void);
void netNtpTick(void);
void netNtpRequestSync(void);
bool netNtpIsSynced(void);
bool netNtpIsWaiting(void);
const char* netNtpStatus(void);

#ifdef __cplusplus
}
#endif

#endif
