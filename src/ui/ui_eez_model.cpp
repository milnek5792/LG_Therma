// ui_eez_model.cpp — EEZ model + sync z LIN
#include "ui_eez_model.h"

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

}  // namespace

void uiEezInit() {
  memset(&uiEez, 0, sizeof(uiEez));
  uiEez.teplota_vody_set = 42.0f;
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
  strncpy(uiEez.cas_text, "--:--", sizeof(uiEez.cas_text));
  strncpy(uiEez.datum_text, "--.--.----", sizeof(uiEez.datum_text));
  uiEez.cas_platny = false;
  strncpy(uiEez.plan_text,
          "Pracovní dny (Po–Pá) • Příští změna ve 22:00",
          sizeof(uiEez.plan_text));
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

  uint8_t b2 = lgModelA0Bajt(2);
  uint8_t b3 = lgModelA0Bajt(3);
  // LIN „live“ jen když A0 přišlo nedávno — jinak vstup/výstup = off
  const bool maA0 = lgMaCerstoA0(5000);

  uint8_t cilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  if (cilova < 15) {
    cilova = 40;
  }

  // Setpoint držíme z UI / last write — nezávisí na LIN online
  uiEez.teplota_vody_set = static_cast<float>(cilova);
  uiEez.teplota_vody_vstup = uiTeplotaC(mVstupni, maA0 && mVstupni > 0);
  uiEez.teplota_vody_vystup = uiTeplotaC(mVystupni, maA0 && mVystupni > 0);

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
  uiEez.sig_kompresor = maA0 && lgJeStabilniBeh(b3);
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
