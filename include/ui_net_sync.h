#ifndef UI_NET_SYNC_H
#define UI_NET_SYNC_H

#include "ui_eez_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** NVS + Wi-Fi init (volat před / kolem UI). */
void uiNetInit(void);

/** Spusť FreeRTOS task pro Wi-Fi/NTP (mimo LVGL jádro). */
void uiNetStartTask(void);

/** Legacy tick — dnes jen sync modelu (síť běží v tasku). */
void uiNetTick(void);

/** Sync Wi-Fi stavů do modelu (settings / hlavní signálky). */
void uiNetSyncWifi(void);

/** Zpracuj Wi-Fi akce z nastavení / formuláře. Vrací true pokud šlo o Wi-Fi akci. */
bool uiNetHandleAction(UiAkceTlacitko akce);

#ifdef __cplusplus
}
#endif

#endif
