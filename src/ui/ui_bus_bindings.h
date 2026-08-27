// ui_bus_bindings.h — UI ↔ LIN
//
// Priority (varianta A):
//   Auto:  voda = jen regulátor; pokoj = HMI|MQTT (last-write-wins); plán = offset/VYP
//   Ruční: voda = HMI|MQTT|plán; regulátor vypnutý
//   SP vody (všechny zdroje): pozadavekNaZapis + sp_pending → C0 33, potvrzení A0 B8
//   Power STOP vždy z HMI / MQTT / plánu
#ifndef UI_BUS_BINDINGS_H
#define UI_BUS_BINDINGS_H

#include "app_cmd.h"
#include "ui_eez_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uiBusHandleAkce(UiAkceTlacitko akce);

/** Absolutní SP vody °C ze zdroje (brána podle režimu). */
void uiBusSetWaterSp(uint8_t teplotaC, UiSpSource src);
/** Regulátor → SP vody (alias UI_SP_SRC_REGULATOR). */
void uiBusSetSetpointC(uint8_t teplotaC);
/** Relativní změna — Auto: pokoj (HMI semantika); ruční: voda. */
void uiBusAdjustSetpoint(int deltaC);

/**
 * MQTT (core 0) nesmí volat LIN přímo — fronta, vykoná uiBusBindingsTick (loop/core 1).
 * start=true → START, start=false → STOP.
 */
void uiBusQueuePower(bool start);
void uiBusQueueSetpointC(uint8_t teplotaC);
void uiBusQueueAdjustSetpoint(int deltaC);

/** Nastaví a zapamatuje režim Auto / ruční. */
bool uiBusSetRegulationAuto(bool enable);

/** Uloží uiEez.rezim do NVS. */
void uiBusPersistRezim(void);

void uiBusBindingsTick(void);
/** NVS session — odloženě, volat až po uiLvglTick (flash neblokuje VSYNC). */
void uiBusFlushDeferredStorage(void);

/** Zpracování ctrl zpráv z appCmdDrainCtrl (LIN / SP / power). */
void uiBusProcessAppMsg(const AppMsg* msg);

/** Volání z časového plánu. */
void uiBusPlanApplyStart(void);
void uiBusPlanApplyStop(void);
void uiBusPlanApplySetpoint(uint8_t teplotaC);

#ifdef __cplusplus
}
#endif

#endif
