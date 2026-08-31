// ui_eez_vars.cpp — EEZ native variables -> uiEez model
#include "ui_eez_vars.h"
#include "ui_eez_model.h"
#include "app_cmd.h"
#include "climate_room.h"
#include "climate_regulator.h"
#include "net_wifi_mgr.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static char s_fmtPool[10][28];
static uint8_t s_fmtPoolIdx = 0;

static char* fmtSlot() {
  char* slot = s_fmtPool[s_fmtPoolIdx++ % (sizeof(s_fmtPool) / sizeof(s_fmtPool[0]))];
  slot[0] = '\0';
  return slot;
}

static bool mqttSignalBleWait(void) {
  if (!uiEez.set_mqtt_enabled) {
    return false;
  }
  return climateRoomIsBusy() || netWifiIsSuspendedForBle();
}

static bool mqttSignalConnecting(void) {
  return uiEez.set_mqtt_enabled && uiEez.sig_wifi &&
         strstr(uiEez.set_mqtt_status, "ipoj") != nullptr;
}

static bool wifiSignalConnecting(void) {
  if (!uiEez.set_wifi_enabled) {
    return false;
  }
  if (netWifiIsBusy() || netWifiIsSuspendedForBle()) {
    return true;
  }
  const char* st = uiEez.set_wifi_status;
  return strstr(st, "ipoj") != nullptr || strstr(st, "Obnovuji") != nullptr ||
         strstr(st, "BLE scan") != nullptr;
}

static uint32_t wifiSignalColor(void) {
  if (uiEez.sig_wifi) {
    return 0x30D158u;
  }
  if (wifiSignalConnecting()) {
    return 0xFF9F0Au;
  }
  return 0x8E8E93u;
}

static uint32_t mqttSignalColor(void) {
  if (uiEez.sig_mqtt) {
    return 0x30D158u;
  }
  if (mqttSignalBleWait()) {
    return 0x0A84FFu;
  }
  if (mqttSignalConnecting()) {
    return 0xFF9F0Au;
  }
  return 0x8E8E93u;
}

static bool tempOffline(float c) {
  return c <= UI_TEPLOTA_NEPLATNA + 100.0f;
}

static const char* fmtTemp(float c) {
  if (tempOffline(c)) {
    return "___";
  }
  char* buf = fmtSlot();
  snprintf(buf, 28, "%d", (int)lroundf(c));
  return buf;
}

static const char* fmtTempDeci(float c) {
  if (tempOffline(c)) {
    return "___";
  }
  char* buf = fmtSlot();
  snprintf(buf, 28, "%.1f", c);
  return buf;
}

static const char* fmtTempUnit(float c) {
  if (tempOffline(c)) {
    return "___";
  }
  char* buf = fmtSlot();
  snprintf(buf, 28, "%.1f°C", c);
  return buf;
}

extern "C" {

const char* get_var_teplota_vody_set() {
  if (uiEez.rezim == UI_REZIM_AUTO) {
    return fmtTempDeci(climateRegulatorRoomSpEffective());
  }
  return fmtTemp(uiEez.teplota_vody_set);
}
void set_var_teplota_vody_set(float value) { uiEez.teplota_vody_set = value; }

uint32_t get_var_teplota_vody_set_color(void) {
  return uiEezTeplotaVodySetColor();
}

const char* get_var_teplota_vody_set_lin() {
  return fmtTemp(uiEez.teplota_vody_set);
}

const char* get_var_teplota_vody_vstup() { return fmtTemp(uiEez.teplota_vody_vstup); }
void set_var_teplota_vody_vstup(float value) { uiEez.teplota_vody_vstup = value; }

const char* get_var_teplota_vody_vystup() { return fmtTemp(uiEez.teplota_vody_vystup); }
void set_var_teplota_vody_vystup(float value) { uiEez.teplota_vody_vystup = value; }

const char* get_var_teplota_vnitrni() { return fmtTempDeci(uiEez.teplota_vnitrni); }
void set_var_teplota_vnitrni(float value) { uiEez.teplota_vnitrni = value; }

const char* get_var_teplota_venkovni() { return fmtTempDeci(uiEez.teplota_venkovni); }
void set_var_teplota_venkovni(float value) { uiEez.teplota_venkovni = value; }

float get_var_teplota_spad() { return uiEez.teplota_spad; }
void set_var_teplota_spad(float value) { uiEez.teplota_spad = value; }

int32_t get_var_rezim() { return (int32_t)uiEez.rezim; }
void set_var_rezim(int32_t value) { uiEez.rezim = (UiRezimRegulace)value; }

int32_t get_var_stav_tc() { return (int32_t)uiEez.stav_tc; }
void set_var_stav_tc(int32_t value) { uiEez.stav_tc = (UiStavTc)value; }

bool get_var_sig_chod() { return uiEez.sig_chod; }
void set_var_sig_chod(bool value) { uiEez.sig_chod = value; }

bool get_var_sig_cerpadlo() { return uiEez.sig_cerpadlo; }
void set_var_sig_cerpadlo(bool value) { uiEez.sig_cerpadlo = value; }

bool get_var_sig_kompresor() { return uiEez.sig_kompresor; }
void set_var_sig_kompresor(bool value) { uiEez.sig_kompresor = value; }

bool get_var_sig_el_topeni() { return uiEez.sig_el_topeni; }
void set_var_sig_el_topeni(bool value) { uiEez.sig_el_topeni = value; }

bool get_var_sig_odmrazovani() { return uiEez.sig_odmrazovani; }
void set_var_sig_odmrazovani(bool value) { uiEez.sig_odmrazovani = value; }

bool get_var_sig_wifi() { return uiEez.sig_wifi; }
void set_var_sig_wifi(bool value) { uiEez.sig_wifi = value; }

bool get_var_sig_mqtt() { return uiEez.sig_mqtt; }
void set_var_sig_mqtt(bool value) { uiEez.sig_mqtt = value; }

bool get_var_sig_ble() { return uiEez.sig_ble; }
void set_var_sig_ble(bool value) { uiEez.sig_ble = value; }

bool get_var_sig_utlum() { return uiEez.sig_utlum; }
void set_var_sig_utlum(bool value) { uiEez.sig_utlum = value; }

bool get_var_sig_alarm() { return uiEez.sig_alarm; }
void set_var_sig_alarm(bool value) { uiEez.sig_alarm = value; }

const char* get_var_cas_text() { return uiEez.cas_text; }
void set_var_cas_text(const char* value) {
  if (value) {
    snprintf(uiEez.cas_text, sizeof(uiEez.cas_text), "%s", value);
  }
}

const char* get_var_datum_text() { return uiEez.datum_text; }
void set_var_datum_text(const char* value) {
  if (value) {
    snprintf(uiEez.datum_text, sizeof(uiEez.datum_text), "%s", value);
  }
}

bool get_var_cas_platny() { return uiEez.cas_platny; }
void set_var_cas_platny(bool value) { uiEez.cas_platny = value; }

int32_t get_var_akce_tlacitko() { return (int32_t)UI_AKCE_ZADNA; }
void set_var_akce_tlacitko(int32_t value) {
  appCmdEnqueueHmi((UiAkceTlacitko)value);
}

const char* get_var_plan_text() { return uiEez.plan_text; }
void set_var_plan_text(const char* value) {
  if (value) {
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "%s", value);
  }
}

const char* get_var_plan_title() { return uiEez.plan_title; }
void set_var_plan_title(const char* value) {
  if (value) {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "%s", value);
  }
}

const char* get_var_sig_wifi____wi_fi__ok_____wi_fi______() {
  if (uiEez.sig_wifi) {
    return "Wi-Fi: OK";
  }
  if (wifiSignalConnecting()) {
    return "Wi-Fi: ...";
  }
  return "Wi-Fi: ---";
}

uint32_t get_var_sig_wifi_color(void) {
  return wifiSignalColor();
}

const char* get_var_sig_mqtt____mqtt__pripojeno_____mqtt______() {
  if (uiEez.sig_mqtt) {
    return "MQTT: OK";
  }
  if (mqttSignalBleWait() || mqttSignalConnecting()) {
    return "MQTT: ...";
  }
  return "MQTT: ---";
}

bool get_var_sig_mqtt_ble_wait(void) {
  return mqttSignalBleWait();
}

uint32_t get_var_sig_mqtt_color(void) {
  return mqttSignalColor();
}

uint32_t get_var_sig_chod___3199320___0x2c2c2e() {
  return uiEez.sig_chod ? 0x30D158u : 0xAEAEB2u;
}

int32_t get_var_sig_chod___255___50() {
  return uiEez.sig_chod ? 255 : 220;
}

uint32_t get_var_sig_cerpadlo___3199320___0x2c2c2e() {
  return uiEez.sig_cerpadlo ? 0x30D158u : 0xAEAEB2u;
}

int32_t get_var_sig_cerpadlo___255___50() {
  return uiEez.sig_cerpadlo ? 255 : 220;
}

uint32_t get_var_sig_kompresor___16752394___0x2c2c2e() {
  return uiEez.sig_kompresor ? 0x30D158u : 0xAEAEB2u;
}

int32_t get_var_sig_kompresor___255___50() {
  return uiEez.sig_kompresor ? 255 : 220;
}

uint32_t get_var_sig_odmrazovani___3199320___0x2c2c2e() {
  return uiEez.sig_odmrazovani ? 0xFF9F0Au : 0xAEAEB2u;
}

int32_t get_var_sig_odmrazovani___255___50() {
  return uiEez.sig_odmrazovani ? 255 : 220;
}

uint32_t get_var_sig_el_topeni___16752394___0x2c2c2e() {
  return uiEez.sig_el_topeni ? 0xFF9F0Au : 0xAEAEB2u;
}

int32_t get_var_sig_el_topeni___255___50() {
  return uiEez.sig_el_topeni ? 255 : 220;
}

}  // extern "C"
