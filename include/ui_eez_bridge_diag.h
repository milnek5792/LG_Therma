// ui_eez_bridge_diag.h — plovoucí diagnostika C3 bridge
#ifndef UI_EEZ_BRIDGE_DIAG_H
#define UI_EEZ_BRIDGE_DIAG_H

#include "lg_lvgl.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* modal_bg;
  lv_obj_t* modal_panel;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_mac;
  lv_obj_t* lbl_ch;
  lv_obj_t* lbl_espnow;
  lv_obj_t* lbl_pwr;
  lv_obj_t* lbl_ota;
  lv_obj_t* btn_refresh;
  lv_obj_t* btn_ota;
  lv_obj_t* btn_close;
} bridge_diag_objects_t;

extern bridge_diag_objects_t bridgeDiagObj;

void uiBridgeDiagCreate(void);
void uiBridgeDiagShow(void);
void uiBridgeDiagHide(void);
bool uiBridgeDiagIsOpen(void);
void uiBridgeDiagTick(void);
lv_obj_t* uiBridgeDiagHitTest(int tx, int ty);

#ifdef __cplusplus
}
#endif

#endif
