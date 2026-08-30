#if 0  // nahrazeno ui_bus_bindings + app_cmd (unified 7B architektura)
#include "ui_ui_bindings.h"
#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "climate_room_uart.h"
#include "climate_scheduler.h"
#include "net_mqtt_client.h"
#include <stdio.h>
#include "net_ntp_time.h"
#include "net_sdio_arbiter.h"
#include "net_wifi_mgr.h"
#include "ui_eez_nav.h"
#include "ui_text_ui.h"
#if LG_USE_EEZ_LVGL
#include "ui_ui_lvgl.h"
#endif

#include <Arduino.h>
#include <string.h>

static UiRezimRegulace ulozenyRezim = UI_REZIM_VYSTUPNI_TEPLOTA;

void uiBindingsInitEarly() {
  uiEezInit();
  climateSchedulerInit();
  ulozenyRezim = uiEez.rezim;
}

void uiBindingsInitNet() {
  netSdioInit();
  netWifiInit();
  netNtpInit();
  climateRoomInit();
  netMqttInit();
}

void uiBindingsInit() {
  uiBindingsInitEarly();
  uiBindingsInitNet();
}

static void uiSettingsSyncNet() {
  uiEez.set_wifi_enabled = netWifiIsEnabled();
  uiEez.sig_wifi = netWifiIsConnected();
  strncpy(uiEez.set_wifi_status, netWifiStatus(), sizeof(uiEez.set_wifi_status));
  strncpy(uiEez.set_wifi_ssid, netWifiSsid(), sizeof(uiEez.set_wifi_ssid));
  strncpy(uiEez.set_wifi_ip, netWifiIp(), sizeof(uiEez.set_wifi_ip));

  uiEez.set_mqtt_enabled = netMqttIsEnabled();
  uiEez.sig_mqtt = netMqttIsConnected();
  // Jeden zdroj pravdy: status string z MQTT workeru (stejný jako na hlavní)
  strncpy(uiEez.set_mqtt_status, netMqttStatus(), sizeof(uiEez.set_mqtt_status));
  uiEez.set_mqtt_status[sizeof(uiEez.set_mqtt_status) - 1] = '\0';
  strncpy(uiEez.set_mqtt_host, netMqttHost(), sizeof(uiEez.set_mqtt_host));

  if (climateRoomIsOk()) {
    snprintf(uiEez.set_sys_hint, sizeof(uiEez.set_sys_hint),
             "H2 %.1f C · bat %d%% · rssi %d",
             climateRoomTempC(), climateRoomBatteryPct(), climateRoomRssi());
  } else if (uiEez.sig_mqtt) {
    snprintf(uiEez.set_sys_hint, sizeof(uiEez.set_sys_hint),
             "MQTT OK · cekam H2 UART");
  } else {
    snprintf(uiEez.set_sys_hint, sizeof(uiEez.set_sys_hint),
             "Cekam H2 na G6/G7");
  }
}

void uiBindingRezimPrepnout() {
  uiEez.rezim = (uiEez.rezim == UI_REZIM_AUTO)
      ? UI_REZIM_VYSTUPNI_TEPLOTA
      : UI_REZIM_AUTO;
  ulozenyRezim = uiEez.rezim;
}

static void uiProvedStartStop() {
  lgModelLock();
  uint8_t b2 = lgModelA0Bajt(2);
  uint8_t b3 = lgModelA0Bajt(3);
  bool tcBezi = lgJeTcProvoz(b2, b3);
  uint8_t aktualniCilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  lgModelUnlock();

  if (tcBezi) {
    if (lgZapisBezi()) { return; }
    if (cekameNaOrigStart) { lgUkonciCekaniProStop(); }
    tcPozadavekZap = false;
    stavZapnuto = false;
    lgNastavDrzenyStav(aktualniCilova, false);
    provedZapisTeploty(aktualniCilova, false, true);
    potrebaObnovitDisplej = true;
    return;
  }

  if (cekameNaOrigStart) {
    lgZrusCekaniOrig("storno z EEZ");
    potrebaObnovitDisplej = true;
    return;
  }

  lgModelLock();
  aktualniCilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  novaCilovaTeplota = aktualniCilova;
  mCilova = aktualniCilova;
  tcPozadavekZap = true;
  lgNastavDrzenyStav(aktualniCilova, true);
  lgModelUnlock();

  nastavStavovyText(TXT_STAV_CEKAM_ORIG);
  provedZapisTeploty(aktualniCilova, true, true);
  potrebaObnovitDisplej = true;
}

static void uiProvedTeplotaZmena(int delta) {
  lgModelLock();
  uint8_t aktualniCilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  if (aktualniCilova < 15) {
    aktualniCilova = mCilova;
  }
  int nova = (int)aktualniCilova + delta;
  if (nova < 15 || nova > 65) {
    lgModelUnlock();
    Serial.printf("[UI] teplota limit (%d)\n", nova);
    return;
  }
  novaCilovaTeplota = (uint8_t)nova;
  mCilova = novaCilovaTeplota;
  pozadavekNaZapis = true;
  pozadavekZmenaStartu = false;
  lgModelUnlock();

  uiEez.teplota_vody_set = (float)novaCilovaTeplota;
  Serial.printf("[UI] teplota %u -> %u C (ceka A0 zapis)\n",
                (unsigned)aktualniCilova, (unsigned)novaCilovaTeplota);
  potrebaObnovitDisplej = true;
}

static void uiSettingsWifiToggle() {
  netWifiSetEnabled(!netWifiIsEnabled());
  if (!netWifiIsEnabled()) {
    netMqttSetEnabled(false);
  }
  uiSettingsSyncNet();
}

static void uiSettingsWifiConnect() {
  if (!netWifiIsEnabled()) {
    netWifiSetEnabled(true);
  }
  netWifiConnect();
  uiSettingsSyncNet();
}

static void uiSettingsMqttToggle() {
  netMqttSetEnabled(!netMqttIsEnabled());
  uiSettingsSyncNet();
}

static void uiSettingsMqttConnect() {
  Serial.println("[UI] MQTT Pripojit");
  netMqttConnect();
  uiSettingsSyncNet();
}

static void uiSettingsPlaceholder(const char* sekce) {
  char hint[64];
  snprintf(hint, sizeof(hint), "%s — pripravuje se", sekce);
  strncpy(uiEez.set_sys_hint, hint, sizeof(uiEez.set_sys_hint));
  Serial.printf("[UI] nastaveni %s — zatim neimplementovano\n", sekce);
}

void uiBindingStartStop() {
  uiProvedStartStop();
}

void uiBindingTeplotaPlus() {
  uiProvedTeplotaZmena(1);
}

void uiBindingTeplotaMinus() {
  uiProvedTeplotaZmena(-1);
}

void uiBindingsZpracujAkci(UiAkceTlacitko akce) {
  switch (akce) {
    case UI_AKCE_START_STOP:
      uiBindingStartStop();
      break;
    case UI_AKCE_TEPLOTA_PLUS:
      uiBindingTeplotaPlus();
      break;
    case UI_AKCE_TEPLOTA_MINUS:
      uiBindingTeplotaMinus();
      break;
    case UI_AKCE_REZIM_PREPNOUT:
      uiBindingRezimPrepnout();
      break;
    case UI_AKCE_TICHY_REZIM:
      climateTichyManualToggle();
      break;
    case UI_AKCE_MENU:
      uiNavigateTo(SCREEN_ID_SETTINGS);
      Serial.println("[UI] obrazovka Nastaveni");
      break;
    case UI_AKCE_ZPET:
      uiNavigateTo(SCREEN_ID_MAIN);
      Serial.println("[UI] obrazovka Hlavni");
      break;
    case UI_AKCE_WIFI_TOGGLE:
      uiSettingsWifiToggle();
      break;
    case UI_AKCE_WIFI_CONNECT:
      uiSettingsWifiConnect();
      break;
    case UI_AKCE_WIFI_EDIT:
      uiNavigateTo(SCREEN_ID_WIFI_SETUP);
      Serial.println("[UI] obrazovka Wi-Fi sit");
      break;
    case UI_AKCE_WIFI_FORM_CONNECT:
      uiSettingsWifiConnect();
      break;
    case UI_AKCE_MQTT_TOGGLE:
      uiSettingsMqttToggle();
      break;
    case UI_AKCE_MQTT_CONNECT:
      uiSettingsMqttConnect();
      break;
    case UI_AKCE_SETTINGS_BLE: {
      char hint[64];
      if (climateRoomIsOk()) {
        snprintf(hint, sizeof(hint), "H2 OK · %.1f C · bat %d%%",
                 climateRoomTempC(), climateRoomBatteryPct());
      } else {
        snprintf(hint, sizeof(hint), "H2 UART G6/G7 — cekam data");
      }
      strncpy(uiEez.set_sys_hint, hint, sizeof(uiEez.set_sys_hint));
      uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
      Serial.printf("[UI] ROOM: ok=%d T=%.1f rssi=%d\n",
                    (int)climateRoomIsOk(), climateRoomTempC(),
                    climateRoomRssi());
      break;
    }
    case UI_AKCE_SETTINGS_PLAN:
      uiSettingsPlaceholder("Casovy plan");
      break;
    case UI_AKCE_SETTINGS_SERVIS:
      uiSettingsPlaceholder("Servis");
      break;
    default:
      break;
  }
}

void uiBindingsTick() {
  UiAkceTlacitko akce = uiEez.akce_tlacitko;
  if (akce != UI_AKCE_ZADNA) {
    uiEez.akce_tlacitko = UI_AKCE_ZADNA;
    uiBindingsZpracujAkci(akce);
  }

  // VŽDY — jinak po TLS freeze nikdy nevyprší (UI zůstane zamrzlé).
  netSdioTick();
  // Stav MQTT/Wi‑Fi i během TLS — ať jde vidět „Pripojovani…“ / fail
  uiSettingsSyncNet();

  if (netSdioTlsBusy()
#if LG_USE_EEZ_LVGL
      || uiLvglIsFrozen()
#endif
  ) {
    climateRoomTick();
    netMqttTick();
    return;
  }

  netWifiTick();
  climateRoomTick();
  netMqttTick();
  netNtpTick();
  climateSchedulerTick();
  uiEezSyncFromBus();
}
#endif  // legacy ui_ui_bindings
