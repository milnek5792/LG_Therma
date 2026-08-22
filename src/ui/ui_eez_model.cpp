// ui_eez_model.cpp — EEZ model + sync z LIN
#include "ui_eez_model.h"

#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"

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
  lgModelLock();

  // LIN „live“ — A0 mladší než LG_A0_FRESH_MS (default 1 min)
  // (lgMaCerstoA0 / A0Bajt berou recursive mutex — OK)
  const bool maA0 = lgMaCerstoA0();
  uint8_t b2 = lgModelA0Bajt(2);
  uint8_t b3 = lgModelA0Bajt(3);

  uint8_t cilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  // Venkovní T jen z BLE outdoor — LIN A0 B5 se nepoužívá

  if (uiEez.rezim != UI_REZIM_AUTO) {
    const uint8_t a0Sp = maA0 ? lgModelA0Bajt(8) : 0;
    const bool a0SpPlatny = maA0 && a0Sp >= 15 && a0Sp <= 65;

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
  } else if (maA0) {
    const uint8_t a0Sp = lgModelA0Bajt(8);
    if (a0Sp >= 15 && a0Sp <= 65) {
      uiEez.teplota_vody_set = static_cast<float>(a0Sp);
    } else if (cilova >= 15 && cilova <= 65) {
      uiEez.teplota_vody_set = static_cast<float>(cilova);
    }
  } else if (cilova < 15) {
    uiEez.teplota_vody_set = UI_TEPLOTA_NEPLATNA;
  }

  uiEez.teplota_vody_vstup = uiTeplotaC(mVstupni, maA0 && mVstupni > 0);
  uiEez.teplota_vody_vystup = uiTeplotaC(mVystupni, maA0 && mVystupni > 0);
  // uiEez.teplota_venkovni nastavuje climate_ble_room

  if (maA0 && mVstupni > 0 && mVystupni > 0) {
    uiEez.teplota_spad = (float)((int)mVystupni - (int)mVstupni);
    if (uiEez.teplota_spad < 0) {
      uiEez.teplota_spad = -uiEez.teplota_spad;
    }
  } else {
    uiEez.teplota_spad = UI_TEPLOTA_NEPLATNA;
  }

  // ZAPNUTO = držený režim od START do STOP (čerpadlo může přijít se zpožděním)
  const bool drzenyZap =
      cilovyZapnutoTab5 || tcPozadavekZap || cekameNaOrigStart;
  uiEez.sig_chod = drzenyZap;

  uiEez.sig_cerpadlo = maA0 && lgJeCerpadloZap(b2);
  // Kompresor: stabilní 0x0A i rozjezd 0x02 (ne jen StabilniBeh)
  uiEez.sig_kompresor = maA0 && lgJeKompresorBezi(b3);
  uiEez.sig_el_topeni = maA0 && ((b2 & 0x04) != 0);
  uiEez.sig_odmrazovani = maA0 && ((b3 & 0x04) != 0);

  if (!drzenyZap) {
    uiEez.stav_tc = UI_STAV_VYP;
  } else if (cekameNaOrigStart && !(maA0 && lgJeTcProvoz(b2, b3))) {
    uiEez.stav_tc = UI_STAV_CEKAM_ORIG;
  } else if (maA0 && lgJePrestartB3(b3)) {
    uiEez.stav_tc = UI_STAV_PRESTART;
  } else if (maA0 && lgJeTcProvoz(b2, b3)) {
    uiEez.stav_tc = UI_STAV_BEH;
  } else {
    // START už držíme, potvrzení ze sběrnice ještě ne
    uiEez.stav_tc = UI_STAV_PRESTART;
  }

  lgModelUnlock();
}

uint32_t uiEezTeplotaVodySetColor(void) {
  if (uiEez.rezim == UI_REZIM_AUTO || uiEez.sp_pending == 0) {
    return UI_SP_COLOR_OK;
  }
  if ((millis() - uiEez.sp_pending_ms) >= spPendingWarnMs()) {
    return UI_SP_COLOR_WARN;
  }
  return UI_SP_COLOR_PENDING;
}
