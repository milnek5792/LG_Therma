// ui_ui_bindings.h — akce tlacitek EEZ -> LIN model
#ifndef UI_BINDINGS_H
#define UI_BINDINGS_H

#include "ui_eez_model.h"

void uiBindingsInit();
/** Model/UI stav — před LVGL. */
void uiBindingsInitEarly();
/** Wi-Fi / MQTT / BLE — až po prvním LVGL flush. */
void uiBindingsInitNet();
void uiBindingsTick();

// Volat z EEZ action handleru (po exportu LVGL) nebo pri testu:
void uiBindingStartStop();
void uiBindingTeplotaPlus();
void uiBindingTeplotaMinus();
void uiBindingRezimPrepnout();

// Zpracuje uiEez.akce_tlacitko (EEZ nastavi pred volanim tick)
void uiBindingsZpracujAkci(UiAkceTlacitko akce);

#endif
