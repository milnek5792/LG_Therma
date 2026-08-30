#ifndef TASK_UI_H
#define TASK_UI_H

void lgTaskUiStart();
void lgTaskUiInitDisplej();
void uiSerialMonitorPoll();

/** Pozastav/obnov UI task (Core 1) — během MQTT DNS/TLS. */
void lgTaskUiSuspend(void);
void lgTaskUiResume(void);

#endif
