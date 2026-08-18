// ui_bus_bindings.h — UI ↔ LIN
#ifndef UI_BUS_BINDINGS_H
#define UI_BUS_BINDINGS_H

#include "ui_eez_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void uiBusHandleAkce(UiAkceTlacitko akce);
/** Absolutní setpoint °C (MQTT cmd/setpoint) → LIN. */
void uiBusSetSetpointC(uint8_t teplotaC);
/** Relativní změna (±1 …) z aktuální hodnoty — jako +/- na HMI. */
void uiBusAdjustSetpoint(int deltaC);

/**
 * MQTT (core 0) nesmí volat LIN přímo — fronta, vykoná uiBusBindingsTick (loop/core 1).
 * start=true → START, start=false → STOP.
 */
void uiBusQueuePower(bool start);
void uiBusQueueSetpointC(uint8_t teplotaC);
void uiBusQueueAdjustSetpoint(int deltaC);

void uiBusBindingsTick(void);

#ifdef __cplusplus
}
#endif

#endif
