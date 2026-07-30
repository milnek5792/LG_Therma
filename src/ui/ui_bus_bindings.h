// ui_bus_bindings.h — UI ↔ LIN
#ifndef UI_BUS_BINDINGS_H
#define UI_BUS_BINDINGS_H

#include "ui_eez_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void uiBusHandleAkce(UiAkceTlacitko akce);
void uiBusBindingsTick(void);

#ifdef __cplusplus
}
#endif

#endif
