// app_cmd.h — FreeRTOS fronta příkazů (HMI, MQTT → ctrl/UI)
#ifndef APP_CMD_H
#define APP_CMD_H

#include "ui_eez_model.h"

#include <freertos/FreeRTOS.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Zdroj požadavku na SP — řídí brány podle režimu. */
typedef enum {
  UI_SP_SRC_HMI = 0,
  UI_SP_SRC_MQTT,
  UI_SP_SRC_REGULATOR,
  UI_SP_SRC_PLAN,
} UiSpSource;

enum AppCmd : uint8_t {
  APP_CMD_NONE = 0,
  APP_CMD_HMI_ACTION,  // arg = UiAkceTlacitko
  APP_CMD_POWER_START,
  APP_CMD_POWER_STOP,
  /** Auto: pokoj; ruční: voda — řeší se při drain podle uiEez.rezim. */
  APP_CMD_SETPOINT_ABS,
  APP_CMD_SETPOINT_DELTA,
};

struct AppMsg {
  AppCmd cmd;
  int32_t arg;
  UiSpSource src;
};

void appCmdInit(void);

bool appCmdEnqueue(const AppMsg* msg);
bool appCmdEnqueueHmi(UiAkceTlacitko akce);

/** MQTT / síť — stejná fronta jako HMI. */
bool appCmdEnqueuePower(bool start, UiSpSource src);
bool appCmdEnqueueSetpointAbs(int val, UiSpSource src);
bool appCmdEnqueueAdjust(int delta, UiSpSource src);

/** UI kontext (ui_tick): navigace, Wi‑Fi/MQTT formuláře. */
void appCmdDrainUi(void);

/** Control kontext (uiBusBindingsTick): LIN, SP, power. */
void appCmdDrainCtrl(void);

UBaseType_t appCmdQueueWaiting(void);

#ifdef __cplusplus
}
#endif

#endif
