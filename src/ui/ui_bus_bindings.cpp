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

void provedTeplotaAbsolutni(uint8_t nova) {
  if (nova < 15 || nova > 65) {
    return;
  }
  lgModelLock();
  novaCilovaTeplota = nova;
  mCilova = nova;
  const bool zap = drzenyZapnuty() || stavZapnuto;
  lgModelUnlock();

  uiEez.teplota_vody_set = (float)nova;
  ESP_LOGI(TAG, "setpoint -> %u C zap=%d", (unsigned)nova, (int)zap);

  // SOLO: hned TX — nečekej na další A0 (jinak +- / MQTT „nefunguje“)
  if (soloRezimTab5) {
    if (lgZapisBezi()) {
      pozadavekNaZapis = true;
      pozadavekZmenaStartu = false;
      ESP_LOGI(TAG, "setpoint odlozen — zapis bezi");
      return;
    }
    pozadavekNaZapis = false;
    pozadavekZmenaStartu = false;
    provedZapisTeploty(nova, zap, false);
    return;
  }

  pozadavekNaZapis = true;
  pozadavekZmenaStartu = false;
}

void provedTeplotaZmena(int delta) {
  lgModelLock();
  const uint8_t aktualniCilova = aktualniCilovaTeplota();
  const int nova = (int)aktualniCilova + delta;
  lgModelUnlock();
  if (nova < 15 || nova > 65) {
    return;
  }
  provedTeplotaAbsolutni((uint8_t)nova);
}

enum class PendingCmd : uint8_t {
  None = 0,
  Start,
  Stop,
  SetAbs,
  Adjust,
};

volatile PendingCmd s_pending = PendingCmd::None;
volatile int s_pendingVal = 0;

void applyPendingFromMqtt() {
  const PendingCmd cmd = s_pending;
  if (cmd == PendingCmd::None) {
    return;
  }
  s_pending = PendingCmd::None;
  const int val = s_pendingVal;

  switch (cmd) {
    case PendingCmd::Start:
      ESP_LOGI(TAG, "MQTT queue → START");
      provedStart();
      break;
    case PendingCmd::Stop:
      ESP_LOGI(TAG, "MQTT queue → STOP");
      provedStop();
      break;
    case PendingCmd::SetAbs:
      ESP_LOGI(TAG, "MQTT queue → setpoint %d", val);
      provedTeplotaAbsolutni((uint8_t)val);
      break;
    case PendingCmd::Adjust:
      ESP_LOGI(TAG, "MQTT queue → adjust %+d", val);
      provedTeplotaZmena(val);
      break;
    default:
      break;
  }
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

void uiBusSetSetpointC(uint8_t teplotaC) {
  provedTeplotaAbsolutni(teplotaC);
}

void uiBusAdjustSetpoint(int deltaC) {
  provedTeplotaZmena(deltaC);
}

void uiBusQueuePower(bool start) {
  s_pendingVal = 0;
  s_pending = start ? PendingCmd::Start : PendingCmd::Stop;
}

void uiBusQueueSetpointC(uint8_t teplotaC) {
  s_pendingVal = (int)teplotaC;
  s_pending = PendingCmd::SetAbs;
}

void uiBusQueueAdjustSetpoint(int deltaC) {
  s_pendingVal = deltaC;
  s_pending = PendingCmd::Adjust;
}

void uiBusBindingsTick(void) {
  applyPendingFromMqtt();
  uiEezSyncFromBus();
}
