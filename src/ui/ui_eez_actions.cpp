// ui_eez_actions.cpp — EEZ button events -> appCmdQ
#include "ui_eez_actions.h"
#include "app_cmd.h"
#include "lg_lvgl.h"
#include <Arduino.h>

extern "C" {

static void enqueue(UiAkceTlacitko akce) {
  appCmdEnqueueHmi(akce);
}

void action_akce_teplota_plus(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_TEPLOTA_PLUS);
}

void action_akce_teplota_minus(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_TEPLOTA_MINUS);
}

void action_akce_start_stop(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_START_STOP);
}

void action_akce_start(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_START);
}

void action_akce_stop(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_STOP);
}

void action_akce_menu(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_MENU);
}

void action_akce_zpet(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_ZPET);
}

void action_akce_wifi_toggle(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_WIFI_TOGGLE);
}

void action_akce_wifi_connect(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_WIFI_CONNECT);
}

void action_akce_wifi_edit(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_WIFI_EDIT);
}

void action_akce_mqtt_toggle(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_MQTT_TOGGLE);
}

void action_akce_mqtt_connect(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_MQTT_CONNECT);
}

void action_akce_settings_ble(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_SETTINGS_BLE);
}

void action_akce_settings_plan(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_SETTINGS_PLAN);
}

void action_akce_settings_servis(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_SETTINGS_SERVIS);
}

void action_akce_plan_back(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_PLAN_BACK);
}

void action_akce_plan_toggle(lv_event_t* e) {
  (void)e;
  enqueue(UI_AKCE_PLAN_TOGGLE);
}

}  // extern "C"
