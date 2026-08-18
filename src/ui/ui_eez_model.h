// ui_eez_model.h — promenne pro EEZ Studio (graficky editor -> LVGL)
//
// V EEZ: Project -> Variables -> vytvor promenne se STEJNYMI jmeny (EEZ_VAR_*).
// Typy doporucene nize u kazde promenne.
//
#ifndef UI_EEZ_MODEL_H
#define UI_EEZ_MODEL_H

#include <Arduino.h>

#define UI_TEPLOTA_NEPLATNA (-1000.0f)

// --- Rezim regulace (tlacitko Auto / Vystupni teplota) ---
enum UiRezimRegulace : uint8_t {
  UI_REZIM_AUTO = 0,              // adaptivni / pokoj (BLE) — climate_adaptive
  UI_REZIM_VYSTUPNI_TEPLOTA = 1   // primy setpoint teploty vody
};

// --- Stav T/C pro barvu tlacitka Start/Stop ---
enum UiStavTc : uint8_t {
  UI_STAV_VYP = 0,
  UI_STAV_CEKAM_ORIG,
  UI_STAV_PRESTART,
  UI_STAV_BEH
};

// --- Akce z EEZ (tlacitka) — firmware je po zpracovani vynuluje ---
enum UiAkceTlacitko : uint8_t {
  UI_AKCE_ZADNA = 0,
  UI_AKCE_START_STOP,  // legacy toggle
  UI_AKCE_START,
  UI_AKCE_STOP,
  UI_AKCE_TEPLOTA_PLUS,
  UI_AKCE_TEPLOTA_MINUS,
  UI_AKCE_REZIM_PREPNOUT,
  UI_AKCE_TICHY_REZIM,
  UI_AKCE_MENU,
  UI_AKCE_ZPET,
  UI_AKCE_WIFI_TOGGLE,
  UI_AKCE_WIFI_CONNECT,
  UI_AKCE_WIFI_EDIT,
  UI_AKCE_WIFI_FORM_BACK,
  UI_AKCE_WIFI_FORM_SAVE,
  UI_AKCE_WIFI_FORM_CONNECT,
  UI_AKCE_MQTT_TOGGLE,
  UI_AKCE_MQTT_CONNECT,
  UI_AKCE_SETTINGS_BLE,
  UI_AKCE_SETTINGS_PLAN,
  UI_AKCE_SETTINGS_SERVIS,
};

// Jmena promennych v EEZ (Native variables) — kopiruj do EEZ 1:1
#define EEZ_VAR_TEPLOTA_VODY_SET     "teplota_vody_set"
#define EEZ_VAR_TEPLOTA_VODY_VSTUP   "teplota_vody_vstup"
#define EEZ_VAR_TEPLOTA_VODY_VYSTUP  "teplota_vody_vystup"
#define EEZ_VAR_TEPLOTA_VNITRNI      "teplota_vnitrni"
#define EEZ_VAR_TEPLOTA_VENKOVNI     "teplota_venkovni"
#define EEZ_VAR_TEPLOTA_SPAD         "teplota_spad"

#define EEZ_VAR_REZIM                "rezim"           // int enum UiRezimRegulace
#define EEZ_VAR_STAV_TC              "stav_tc"         // int enum UiStavTc

#define EEZ_VAR_SIG_CHOD             "sig_chod"        // bool — T/C v provozu
#define EEZ_VAR_SIG_CERPADLO         "sig_cerpadlo"
#define EEZ_VAR_SIG_KOMPRESOR        "sig_kompresor"
#define EEZ_VAR_SIG_EL_TOPENI        "sig_el_topeni"   // el. patrona
#define EEZ_VAR_SIG_ODMRAZOVANI      "sig_odmrazovani"
#define EEZ_VAR_SIG_WIFI             "sig_wifi"
#define EEZ_VAR_SIG_MQTT             "sig_mqtt"
#define EEZ_VAR_SIG_BLE              "sig_ble"         // navrh: BLE senzor OK
#define EEZ_VAR_SIG_REMOTE           "sig_remote"      // bool — někdo sleduje (MQTT watch)
#define EEZ_VAR_SIG_UTLUM            "sig_utlum"       // bool — tichy rezim T/C (planovany v noci)
#define EEZ_VAR_SIG_ALARM            "sig_alarm"       // navrh: chyba / porucha

#define EEZ_VAR_CAS                  "cas_text"        // string "14:32"
#define EEZ_VAR_DATUM                "datum_text"      // string "20.07.2026"
#define EEZ_VAR_CAS_PLATNY           "cas_platny"      // bool — NTP synchronizovan
#define EEZ_VAR_PLAN_TEXT            "plan_text"       // string — radek planovace (HMI)

#define EEZ_VAR_AKCE                 "akce_tlacitko"   // int enum UiAkceTlacitko (EEZ -> FW)

struct UiEezModel {
  float teplota_vody_set;
  float teplota_vody_vstup;
  float teplota_vody_vystup;
  float teplota_vnitrni;
  float teplota_venkovni;
  float teplota_spad;

  UiRezimRegulace rezim;
  UiStavTc stav_tc;

  bool sig_chod;
  bool sig_cerpadlo;
  bool sig_kompresor;
  bool sig_el_topeni;
  bool sig_odmrazovani;
  bool sig_wifi;
  bool sig_mqtt;
  bool sig_ble;
  bool sig_remote;
  volatile bool sig_utlum;
  bool sig_alarm;

  char cas_text[8];
  char datum_text[12];
  bool cas_platny;
  char plan_text[96];

  char set_wifi_ssid[33];
  char set_wifi_ip[16];
  char set_wifi_status[40];
  char set_mqtt_host[64];
  char set_mqtt_status[40];
  char set_sys_hint[64];
  bool set_wifi_enabled;
  bool set_mqtt_enabled;

  volatile UiAkceTlacitko akce_tlacitko;
};

extern UiEezModel uiEez;

void uiEezInit();
void uiEezSyncFromBus();
void uiEezNastavCas(const char* cas, const char* datum, bool platny);
void uiEezNastavSit(bool wifi, bool mqtt, bool ble);
void uiEezNastavTeplotuVnitrni(float c);
void uiEezNastavTeplotuVenkovni(float c);

#endif
