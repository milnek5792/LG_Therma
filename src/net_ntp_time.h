#ifndef NET_NTP_TIME_H
#define NET_NTP_TIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void netNtpInit();
void netNtpTick();
void netNtpRequestSync();
bool netNtpIsSynced();
bool netNtpIsWaiting();
const char* netNtpStatus();

#ifdef __cplusplus
}
#endif

#endif
