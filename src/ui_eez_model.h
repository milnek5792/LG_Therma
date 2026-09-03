// ui_eez_model.h — promenne pro EEZ Studio (graficky editor -> LVGL)
#ifndef UI_EEZ_MODEL_H
#define UI_EEZ_MODEL_H

#include <Arduino.h>

#include "bus_lg_config.h"

#define UI_TEPLOTA_NEPLATNA (-1000.0f)

#ifndef UI_SP_PENDING_WARN_MS
#define UI_SP_PENDING_WARN_MS (LG_A0_SLOW_PERIOD_MS + UI_SP_PENDING_MARGIN_MS)
#endif

#define UI_SP_COLOR_OK      0xFFFFFFu
#define UI_SP_COLOR_PENDING 0xFF9F0Au
#define UI_SP_COLOR_WARN    0xFF453Au

enum UiRezimRegulace : uint8_t {
  UI_REZIM_AUTO = 0,
  UI_REZIM_VYSTUPNI_TEPLOTA = 1
};

enum UiStavTc : uint8_t {
  UI_STAV_VYP = 0,
  UI_STAV_CEKAM_ORIG,
  UI_STAV_PRESTART,
  UI_STAV_BEH
};

enum UiAkceTlacitko : uint8_t {
  UI_AKCE_ZADNA = 0,
  UI_AKCE_START_STOP,
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
  UI_AKCE_SETTINGS_METER1,
  UI_AKCE_SETTINGS_METER2,
  UI_AKCE_SETTINGS_METER3,
  UI_AKCE_SETTINGS_BLE_MAC,
  UI_AKCE_SETTINGS_BRIDGE_OTA,
  UI_AKCE_SETTINGS_BRIDGE_DIAG,
  UI_AKCE_BLE_MAC_BACK,
  UI_AKCE_BLE_MAC_SAVE,
  UI_AKCE_SETTINGS_PLAN,
  UI_AKCE_SETTINGS_SERVIS,
  UI_AKCE_SETTINGS_SPOTREBA,
  UI_AKCE_PLAN_BACK,
  UI_AKCE_PLAN_TOGGLE,
};

#define EEZ_VAR_TEPLOTA_VODY_SET     "teplota_vody_set"
#define EEZ_VAR_TEPLOTA_VODY_VSTUP   "teplota_vody_vstup"
#define EEZ_VAR_TEPLOTA_VODY_VYSTUP  "teplota_vody_vystup"
#define EEZ_VAR_TEPLOTA_VNITRNI      "teplota_vnitrni"
#define EEZ_VAR_TEPLOTA_VENKOVNI     "teplota_venkovni"
#define EEZ_VAR_TEPLOTA_SPAD         "teplota_spad"
#define EEZ_VAR_REZIM                "rezim"
#define EEZ_VAR_STAV_TC              "stav_tc"
#define EEZ_VAR_SIG_CHOD             "sig_chod"
#define EEZ_VAR_SIG_CERPADLO         "sig_cerpadlo"
#define EEZ_VAR_SIG_KOMPRESOR        "sig_kompresor"
#define EEZ_VAR_SIG_EL_TOPENI        "sig_el_topeni"
#define EEZ_VAR_SIG_ODMRAZOVANI      "sig_odmrazovani"
#define EEZ_VAR_SIG_TICHY_LIN        "sig_tichy_lin"
#define EEZ_VAR_SIG_WIFI             "sig_wifi"
#define EEZ_VAR_SIG_MQTT             "sig_mqtt"
#define EEZ_VAR_SIG_BLE              "sig_ble"
#define EEZ_VAR_SIG_REMOTE           "sig_remote"
#define EEZ_VAR_SIG_UTLUM            "sig_utlum"
#define EEZ_VAR_SIG_ALARM            "sig_alarm"
#define EEZ_VAR_CAS                  "cas_text"
#define EEZ_VAR_DATUM                "datum_text"
#define EEZ_VAR_CAS_PLATNY           "cas_platny"
#define EEZ_VAR_PLAN_TEXT            "plan_text"
#define EEZ_VAR_PLAN_TITLE           "plan_title"
#define EEZ_VAR_AKCE                 "akce_tlacitko"

struct UiEezModel {
  float teplota_vody_set;
  float teplota_vody_vstup;
  float teplota_vody_vystup;
  float teplota_vnitrni;
  float teplota_venkovni;
  float teplota_spad;

  uint8_t sp_pending;
  uint32_t sp_pending_ms;

  UiRezimRegulace rezim;
  UiStavTc stav_tc;

  bool sig_chod;
  bool sig_cerpadlo;
  bool sig_kompresor;
  bool sig_el_topeni;
  bool sig_odmrazovani;
  bool sig_tichy_lin;
  bool sig_wifi;
  bool sig_mqtt;
  bool sig_ble;
  bool sig_remote;
  volatile bool sig_utlum;
  bool sig_alarm;

  char cas_text[8];
  char datum_text[12];
  bool cas_platny;
  char plan_title[32];
  char plan_text[96];

  char set_wifi_ssid[33];
  char set_wifi_ip[16];
  char set_wifi_status[40];
  char set_mqtt_host[64];
  char set_mqtt_status[40];
  char set_sys_hint[64];
  char porucha_text[80];
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
uint32_t uiEezTeplotaVodySetColor(void);

#endif
