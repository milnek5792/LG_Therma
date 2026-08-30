#ifndef UI_EEZ_WIFI_FORM_H
#define UI_EEZ_WIFI_FORM_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_ssid;
  lv_obj_t* ta_ssid;
  lv_obj_t* lbl_pass;
  lv_obj_t* ta_pass;
  lv_obj_t* btn_save;
  lv_obj_t* btn_connect;
  lv_obj_t* keyboard;
} wifi_form_objects_t;

extern wifi_form_objects_t wifiFormObj;

void uiWifiFormCreate();
void uiWifiFormPrepare();
bool uiWifiFormSaveCredentials();
void uiWifiFormTick();
lv_obj_t* uiWifiFormScreen();

#ifdef __cplusplus
}
#endif

#endif
