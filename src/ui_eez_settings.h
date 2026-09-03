#ifndef UI_EEZ_SETTINGS_H
#define UI_EEZ_SETTINGS_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_version;
  lv_obj_t* panel_wifi;
  lv_obj_t* lbl_wifi_title;
  lv_obj_t* lbl_wifi_status;
  lv_obj_t* lbl_wifi_ssid;
  lv_obj_t* lbl_wifi_ip;
  lv_obj_t* btn_wifi_toggle;
  lv_obj_t* btn_wifi_connect;
  lv_obj_t* btn_wifi_edit;
  lv_obj_t* panel_mqtt;
  lv_obj_t* lbl_mqtt_title;
  lv_obj_t* lbl_mqtt_status;
  lv_obj_t* lbl_mqtt_host;
  lv_obj_t* btn_mqtt_toggle;
  lv_obj_t* btn_mqtt_connect;
  lv_obj_t* panel_display;
  lv_obj_t* lbl_disp_title;
  lv_obj_t* lbl_brightness;
  lv_obj_t* slider_brightness;
  lv_obj_t* lbl_sleep;
  lv_obj_t* btn_sleep;
  lv_obj_t* panel_sys;
  lv_obj_t* panel_ble;
  lv_obj_t* lbl_sys_title;
  lv_obj_t* lbl_sys_hint;
  lv_obj_t* lbl_mac_room;
  lv_obj_t* lbl_mac_out;
  lv_obj_t* lbl_rsp_room;
  lv_obj_t* lbl_rsp_out;
  lv_obj_t* btn_ble;
  lv_obj_t* btn_mac;
  lv_obj_t* btn_bridge;
  lv_obj_t* btn_meter1;
  lv_obj_t* btn_meter2;
  lv_obj_t* btn_meter3;
  lv_obj_t* btn_plan;
  lv_obj_t* btn_servis;
  lv_obj_t* btn_spotreba;
} settings_objects_t;

extern settings_objects_t settingsObj;

void uiSettingsCreate();
void uiSettingsTick();
/** Okamžitá zpětná vazba po stisku Skenuj (volat z LVGL vlákna). */
void uiSettingsShowScanning();
void uiSettingsShowBridgeOtaHint(const char* hint);
void uiSettingsSetBrightnessFromTouch(uint8_t percent, bool persist);
lv_obj_t* uiSettingsScreen();

#ifdef __cplusplus
}
#endif

#endif
