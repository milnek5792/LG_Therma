// ui_eez_model.cpp — EEZ model + sync z LIN
#include "ui_eez_model.h"

#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "storage_config_nvs.h"

#include <cstdio>
#include <cstring>

UiEezModel uiEez;

namespace {

float uiTeplotaC(uint8_t b, bool platna) {
  if (!platna) {
    return UI_TEPLOTA_NEPLATNA;
  }
  return static_cast<float>(b);
}

unsigned long spPendingWarnMs(void) {
  const unsigned long period = lgA0PeriodMs();
  if (period > 0) {
    const unsigned long adaptive = period + UI_SP_PENDING_MARGIN_MS;
    return adaptive > UI_SP_PENDING_WARN_MS ? adaptive : UI_SP_PENDING_WARN_MS;
  }
  return UI_SP_PENDING_WARN_MS;
}

}  // namespace

void uiEezInit() {
  memset(&uiEez, 0, sizeof(uiEez));
  uiEez.teplota_vody_set = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_vody_vstup = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_vody_vystup = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_vnitrni = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_venkovni = UI_TEPLOTA_NEPLATNA;
  uiEez.teplota_spad = UI_TEPLOTA_NEPLATNA;
  uiEez.rezim = UI_REZIM_VYSTUPNI_TEPLOTA;
  {
    uint8_t saved = UI_REZIM_VYSTUPNI_TEPLOTA;
    if (storageLoadUiRezim(&saved)) {
      uiEez.rezim = (UiRezimRegulace)saved;
    }
  }
  uiEez.stav_tc = UI_STAV_VYP;
  uiEez.sig_wifi = false;
  uiEez.sig_mqtt = false;
  uiEez.sig_ble = false;
  uiEez.sig_remote = false;
  strncpy(uiEez.cas_text, "--:--", sizeof(uiEez.cas_text));
  strncpy(uiEez.datum_text, "--.--.----", sizeof(uiEez.datum_text));
  uiEez.cas_platny = false;
  strncpy(uiEez.plan_title, "PLAN VYPNUTY", sizeof(uiEez.plan_title));
  strncpy(uiEez.plan_text, "Casovy plan je neaktivni", sizeof(uiEez.plan_text));
  strncpy(uiEez.set_wifi_ssid, "—", sizeof(uiEez.set_wifi_ssid));
  strncpy(uiEez.set_wifi_ip, "—", sizeof(uiEez.set_wifi_ip));
  strncpy(uiEez.set_wifi_status, "Odpojeno", sizeof(uiEez.set_wifi_status));
  strncpy(uiEez.set_mqtt_host, "—", sizeof(uiEez.set_mqtt_host));
  strncpy(uiEez.set_mqtt_status, "Odpojeno", sizeof(uiEez.set_mqtt_status));
  strncpy(uiEez.set_sys_hint, "Cekam na senzor...", sizeof(uiEez.set_sys_hint));
  uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
}

void uiEezNastavCas(const char* cas, const char* datum, bool platny) {
  if (cas) {
    snprintf(uiEez.cas_text, sizeof(uiEez.cas_text), "%s", cas);
  }
  if (datum) {
    snprintf(uiEez.datum_text, sizeof(uiEez.datum_text), "%s", datum);
  }
  uiEez.cas_platny = platny;
}

void uiEezNastavSit(bool wifi, bool mqtt, bool ble) {
  uiEez.sig_wifi = wifi;
  uiEez.sig_mqtt = mqtt;
  uiEez.sig_ble = ble;
}

void uiEezNastavTeplotuVnitrni(float c) { uiEez.teplota_vnitrni = c; }
void uiEezNastavTeplotuVenkovni(float c) { uiEez.teplota_venkovni = c; }

void uiEezSyncFromBus() {
  LgModelUiSnap bus{};
  lgModelReadUiSnap(&bus);

  const bool linLive = bus.lin_live;
  const uint8_t b2 = bus.b2;
  const uint8_t b3 = bus.b3;
  const uint8_t cilova =
      bus.pozadavek_zapis ? bus.nova_cilova : bus.m_cilova;

  const uint8_t a0Sp = linLive ? bus.a0_sp : 0;
  const bool a0SpPlatny = linLive && a0Sp >= 15 && a0Sp <= 65;

  if (uiEez.sp_pending != 0) {
    if (a0SpPlatny && a0Sp == uiEez.sp_pending) {
      uiEez.sp_pending = 0;
      uiEez.teplota_vody_set = static_cast<float>(a0Sp);
    } else {
      uiEez.teplota_vody_set = static_cast<float>(uiEez.sp_pending);
    }
  } else if (a0SpPlatny) {
    uiEez.teplota_vody_set = static_cast<float>(a0Sp);
  } else if (cilova >= 15 && cilova <= 65) {
    uiEez.teplota_vody_set = static_cast<float>(cilova);
  } else {
    uiEez.teplota_vody_set = UI_TEPLOTA_NEPLATNA;
  }

  // Po STOP A0 často pošle B11/B12=0 — drž poslední platné, dokud LIN žije.
  static uint8_t s_holdVstup = 0;
  static uint8_t s_holdVystup = 0;
  if (linLive) {
    if (bus.m_vstupni > 0) {
      s_holdVstup = bus.m_vstupni;
    }
    if (bus.m_vystupni > 0) {
      s_holdVystup = bus.m_vystupni;
    }
  }
  const uint8_t vstupShow =
      (bus.m_vstupni > 0) ? bus.m_vstupni : s_holdVstup;
  const uint8_t vystupShow =
      (bus.m_vystupni > 0) ? bus.m_vystupni : s_holdVystup;
  const bool teplotaPlatna = linLive && vstupShow > 0;
  uiEez.teplota_vody_vstup = uiTeplotaC(vstupShow, teplotaPlatna);
  uiEez.teplota_vody_vystup = uiTeplotaC(vystupShow, teplotaPlatna);

  if (linLive && vstupShow > 0 && vystupShow > 0) {
    uiEez.teplota_spad = (float)((int)vystupShow - (int)vstupShow);
    if (uiEez.teplota_spad < 0) {
      uiEez.teplota_spad = -uiEez.teplota_spad;
    }
  } else {
    uiEez.teplota_spad = UI_TEPLOTA_NEPLATNA;
  }

  uiEez.sig_chod =
      bus.cilovy_zapnuto || bus.cekame_orig || bus.tc_pozadavek;

  uiEez.sig_cerpadlo = linLive && lgJeCerpadloZap(b2);
  uiEez.sig_kompresor = linLive && lgJeKompresorBezi(b3);
  uiEez.sig_el_topeni = linLive && ((b2 & 0x04) != 0);
  uiEez.sig_odmrazovani = linLive && ((b3 & 0x04) != 0);

  if (!bus.cilovy_zapnuto && !bus.cekame_orig && !bus.tc_pozadavek) {
    uiEez.stav_tc = UI_STAV_VYP;
  } else if (bus.cekame_orig && !(linLive && lgJeTcProvoz(b2, b3))) {
    uiEez.stav_tc = UI_STAV_CEKAM_ORIG;
  } else if (bus.cekame_orig) {
    uiEez.stav_tc = UI_STAV_PRESTART;
  } else if (linLive && (lgJeTcProvoz(b2, b3) || lgJeCerpadloZap(b2))) {
    uiEez.stav_tc = UI_STAV_BEH;
  } else if (bus.cilovy_zapnuto) {
    uiEez.stav_tc = UI_STAV_PRESTART;
  } else {
    uiEez.stav_tc = UI_STAV_VYP;
  }
}

uint32_t uiEezTeplotaVodySetColor(void) {
  if (uiEez.sp_pending == 0) {
    return UI_SP_COLOR_OK;
  }
  if ((millis() - uiEez.sp_pending_ms) >= spPendingWarnMs()) {
    return UI_SP_COLOR_WARN;
  }
  return UI_SP_COLOR_PENDING;
}
