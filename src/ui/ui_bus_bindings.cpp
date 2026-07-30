// ui_bus_bindings.cpp — EEZ akce → LIN zápis + sync model → UI
#include "ui_bus_bindings.h"

#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <esp_log.h>

namespace {

static const char* TAG = "UI_BUS";

uint8_t aktualniCilovaTeplota() {
  uint8_t t = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  if (t < 15) {
    t = mCilova;
  }
  if (t < 15) {
    const int fromUi = (int)(uiEez.teplota_vody_set + 0.5f);
    t = (fromUi >= 15 && fromUi <= 65) ? (uint8_t)fromUi : 40;
  }
  return t;
}

bool drzenyZapnuty() {
  return cilovyZapnutoTab5 || tcPozadavekZap || cekameNaOrigStart;
}

void provedStop() {
  lgModelLock();
  const uint8_t t = aktualniCilovaTeplota();
  lgModelUnlock();

  if (cekameNaOrigStart) {
    lgUkonciCekaniProStop();
  }
  tcPozadavekZap = false;
  stavZapnuto = false;
  lgNastavDrzenyStav(t, false);
  provedZapisTeploty(t, false, true);
  uiEez.sig_chod = false;
  uiEez.stav_tc = UI_STAV_VYP;
  ESP_LOGI(TAG, "STOP T=%u", (unsigned)t);
}

void provedStart() {
  lgModelLock();
  const uint8_t b2 = lgModelA0Bajt(2);
  const uint8_t b3 = lgModelA0Bajt(3);
  const bool uzDrzeny = drzenyZapnuty();
  const bool tcBezi = lgJeTcProvoz(b2, b3);
  uint8_t t = aktualniCilovaTeplota();
  lgModelUnlock();

  if (uzDrzeny || tcBezi) {
    ESP_LOGI(TAG, "START ignorovan — uz zapnuto");
    return;
  }
  if (lgZapisBezi()) {
    ESP_LOGI(TAG, "START ignorovan — zapis bezi");
    return;
  }

  lgModelLock();
  t = aktualniCilovaTeplota();
  novaCilovaTeplota = t;
  mCilova = t;
  tcPozadavekZap = true;
  lgNastavDrzenyStav(t, true);
  lgModelUnlock();

  // UI hned — čerpadlo / A0 může přijít se zpožděním
  uiEez.sig_chod = true;
  uiEez.stav_tc = UI_STAV_PRESTART;
  uiEez.teplota_vody_set = (float)t;

  provedZapisTeploty(t, true, true);
  ESP_LOGI(TAG, "START T=%u (SOLO, drzeny ON)", (unsigned)t);
}

void provedStartStopToggle() {
  if (drzenyZapnuty()) {
    provedStop();
  } else {
    provedStart();
  }
}

void provedTeplotaZmena(int delta) {
  lgModelLock();
  uint8_t aktualniCilova = aktualniCilovaTeplota();
  int nova = (int)aktualniCilova + delta;
  if (nova < 15 || nova > 65) {
    lgModelUnlock();
    return;
  }
  novaCilovaTeplota = (uint8_t)nova;
  mCilova = novaCilovaTeplota;
  const bool zap = drzenyZapnuty() || stavZapnuto;
  lgModelUnlock();

  uiEez.teplota_vody_set = (float)novaCilovaTeplota;
  ESP_LOGI(TAG, "setpoint -> %u C zap=%d", (unsigned)novaCilovaTeplota, (int)zap);

  // SOLO: hned TX — nečekej na další A0 (jinak +- „nefunguje“)
  if (soloRezimTab5) {
    if (lgZapisBezi()) {
      // Fronta: pošli po dokončení aktuálního zápisu přes A0 hook
      pozadavekNaZapis = true;
      pozadavekZmenaStartu = false;
      ESP_LOGI(TAG, "setpoint odlozen — zapis bezi");
      return;
    }
    pozadavekNaZapis = false;
    pozadavekZmenaStartu = false;
    provedZapisTeploty(novaCilovaTeplota, zap, false);
    return;
  }

  pozadavekNaZapis = true;
  pozadavekZmenaStartu = false;
}

}  // namespace

void uiBusHandleAkce(UiAkceTlacitko akce) {
  switch (akce) {
    case UI_AKCE_START:
      provedStart();
      break;
    case UI_AKCE_STOP:
      provedStop();
      break;
    case UI_AKCE_START_STOP:
      provedStartStopToggle();
      break;
    case UI_AKCE_TEPLOTA_PLUS:
      provedTeplotaZmena(1);
      break;
    case UI_AKCE_TEPLOTA_MINUS:
      provedTeplotaZmena(-1);
      break;
    default:
      break;
  }
}

void uiBusBindingsTick(void) {
  uiEezSyncFromBus();
}
