// bus_lg_task.h — FreeRTOS LIN úloha (core 1)
#ifndef BUS_LG_TASK_H
#define BUS_LG_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void lgBusStartTask(void);
bool lgBusTaskRunning(void);

#ifdef __cplusplus
}
#endif

#endif
