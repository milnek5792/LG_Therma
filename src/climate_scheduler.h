#ifndef CLIMATE_SCHEDULER_H
#define CLIMATE_SCHEDULER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void climateSchedulerInit();
void climateSchedulerTick();
void climateTichyManualToggle();

#ifdef __cplusplus
}
#endif

#endif
