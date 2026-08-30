#ifndef UI_EEZ_BLE_MAC_FORM_H
#define UI_EEZ_BLE_MAC_FORM_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_room;
  lv_obj_t* ta_room;
  lv_obj_t* lbl_out;
  lv_obj_t* ta_out;
  lv_obj_t* btn_save;
  lv_obj_t* lbl_hint;
  lv_obj_t* keyboard;
} ble_mac_form_objects_t;

extern ble_mac_form_objects_t bleMacFormObj;

void uiBleMacFormCreate(void);
void uiBleMacFormPrepare(void);
bool uiBleMacFormSave(void);
void uiBleMacFormTick(void);
lv_obj_t* uiBleMacFormScreen(void);

#ifdef __cplusplus
}
#endif

#endif
