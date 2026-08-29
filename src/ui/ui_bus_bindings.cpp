// ui_bus_bindings.cpp — EEZ akce → LIN zápis + sync model → UI
#include "ui_bus_bindings.h"

#include "app_cmd.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "climate_plan.h"
#include "climate_regulator.h"
#include "storage_config_nvs.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <esp_log.h>

namespace {

static const char* TAG = "UI_BUS";

constexpr uint8_t kWaterMinC = REG_T_WATER_MIN_C;
constexpr uint8_t kWaterMaxC = REG_T_WATER_MAX_C;

const char* spSrcName(UiSpSource src) {
  switch (src) {
    case UI_SP_SRC_HMI:
      return "HMI";
    case UI_SP_SRC_MQTT:
      return "MQTT";
    case UI_SP_SRC_REGULATOR:
      return "REG";
    case UI_SP_SRC_PLAN:
      return "PLAN";
    default:
      return "?";
  }
}

/** Varianta A: Auto → jen regulátor; ruční → HMI / MQTT / plán. */
bool allowWaterSpWrite(UiSpSource src) {
  if (uiEez.rezim == UI_REZIM_AUTO) {
    return src == UI_SP_SRC_REGULATOR;
  }
  return src == UI_SP_SRC_HMI || src == UI_SP_SRC_MQTT || src == UI_SP_SRC_PLAN;
}

/** Pokojový SP: HMI nebo MQTT (last-write-wins). */
bool allowRoomSpWrite(UiSpSource src) {
  return src == UI_SP_SRC_HMI || src == UI_SP_SRC_MQTT;
}

uint8_t clampWaterC(int t) {
  if (t < (int)kWaterMinC) {
    return kWaterMinC;
  }
  if (t > (int)kWaterMaxC) {
    return kWaterMaxC;
  }
  return (uint8_t)t;
}

uint8_t aktualniCilovaTeplota() {
  if (pozadavekNaZapis) {
    return clampWaterC(novaCilovaTeplota);
  }
  if (mCilova >= kWaterMinC && mCilova <= kWaterMaxC) {
    return mCilova;
  }
  lgModelLock();
  const uint8_t a0Sp = lgMaCerstoA0() ? lgModelA0Bajt(8) : 0;
  lgModelUnlock();
  if (a0Sp >= kWaterMinC && a0Sp <= kWaterMaxC) {
    return a0Sp;
  }
  return clampWaterC(35);
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

  lgModelLock();
  novaCilovaTeplota = t;
  pozadavekZmenaStartu = true;
  pozadavekNaZapis = true;
  lgModelUnlock();

  uiEez.sig_chod = false;
  uiEez.stav_tc = UI_STAV_VYP;
  ESP_LOGI(TAG, "STOP T=%u (session OFF)", (unsigned)t);
}

void provedStart() {
  lgModelLock();
  const uint8_t b2 = lgModelA0Bajt(2);
  const uint8_t b3 = lgModelA0Bajt(3);
  const bool uzDrzeny = drzenyZapnuty();
  const bool tcBezi = lgJeTcProvoz(b2, b3);
  uint8_t t = aktualniCilovaTeplota();
  lgModelUnlock();

  // Auto: START s cílem regulátoru, ne se starým ručním SP z HMI/A0
  if (uiEez.rezim == UI_REZIM_AUTO) {
    RegulatorSnapshot snap{};
    climateRegulatorGetSnapshot(&snap);
    t = snap.t_water_c;
  }

  if (uzDrzeny && cilovyZapnutoTab5) {
    ESP_LOGI(TAG, "START ignorovan — uz zapnuto");
    return;
  }

  // Po restartu / cizí topný cyklus: převezmi session bez C0 (ne oběhové čerpadlo).
  if (tcBezi) {
    lgModelLock();
    novaCilovaTeplota = t;
    mCilova = t;
    tcPozadavekZap = true;
    lgNastavDrzenyStav(t, true);
    lgModelUnlock();
    uiEez.sig_chod = true;
    uiEez.stav_tc = UI_STAV_BEH;
    ESP_LOGI(TAG, "START adopt (TČ uz bezi) T=%u", (unsigned)t);
    return;
  }

  lgModelLock();
  novaCilovaTeplota = t;
  mCilova = t;
  tcPozadavekZap = true;
  lgNastavDrzenyStav(t, true);
  pozadavekZmenaStartu = true;
  pozadavekNaZapis = true;
  lgModelUnlock();

  uiEez.sig_chod = true;
  uiEez.stav_tc = UI_STAV_PRESTART;

  if (uiEez.rezim == UI_REZIM_AUTO) {
    climateRegulatorRequestImmediateTick();
  }

  ESP_LOGI(TAG, "START T=%u (session ON)%s", (unsigned)t,
           uiEez.rezim == UI_REZIM_AUTO ? " Auto" : "");
}

void provedStartStopToggle() {
  if (drzenyZapnuty()) {
    provedStop();
  } else {
    provedStart();
  }
}

void provedTeplotaAbsolutni(uint8_t nova, UiSpSource src) {
  nova = clampWaterC(nova);
  if (!allowWaterSpWrite(src)) {
    ESP_LOGW(TAG, "SP vody %u blokovan (src=%s rezim=%s)", (unsigned)nova,
             spSrcName(src),
             uiEez.rezim == UI_REZIM_AUTO ? "AUTO" : "MAN");
    return;
  }

  if (!drzenyZapnuty()) {
    ESP_LOGW(TAG, "SP vody %u ignorovan — nejdriv START (src=%s)", (unsigned)nova,
             spSrcName(src));
    return;
  }

  lgModelLock();
  novaCilovaTeplota = nova;
  mCilova = nova;
  if (drzetStavAktivni) {
    cilovaTeplotaTab5 = nova;
  }
  pozadavekZmenaStartu = false;
  pozadavekNaZapis = true;
  lgModelUnlock();

  uiEez.teplota_vody_set = static_cast<float>(nova);
  uiEez.sp_pending = nova;
  uiEez.sp_pending_ms = millis();
  potrebaObnovitDisplej = true;

  storageRequestSaveTcSession(cilovyZapnutoTab5, nova);

  ESP_LOGI(TAG, "setpoint cmd -> %u C src=%s", (unsigned)nova, spSrcName(src));
}

void provedTeplotaZmena(int delta, UiSpSource src) {
  lgModelLock();
  const uint8_t aktualniCilova = aktualniCilovaTeplota();
  const int nova = (int)aktualniCilova + delta;
  lgModelUnlock();
  if (nova < (int)kWaterMinC || nova > (int)kWaterMaxC) {
    return;
  }
  provedTeplotaAbsolutni((uint8_t)nova, src);
}

void provedRoomSpZmena(float deltaC, UiSpSource src) {
  if (!allowRoomSpWrite(src)) {
    ESP_LOGW(TAG, "room SP adjust blokovan (src=%s)", spSrcName(src));
    return;
  }
  climateRegulatorAdjustRoomSp(deltaC);
  ESP_LOGI(TAG, "Auto room SP -> %.1f (src=%s)",
           (double)climateRegulatorGetConfig()->room_sp_c, spSrcName(src));
}

void provedRoomSpAbs(float c, UiSpSource src) {
  if (!allowRoomSpWrite(src)) {
    ESP_LOGW(TAG, "room SP abs blokovan (src=%s)", spSrcName(src));
    return;
  }
  climateRegulatorSetRoomSp(c);
  ESP_LOGI(TAG, "Auto room SP abs -> %.1f (src=%s)",
           (double)climateRegulatorGetConfig()->room_sp_c, spSrcName(src));
}

void processAppMsg(const AppMsg& msg) {
  const bool autoMode = (uiEez.rezim == UI_REZIM_AUTO);

  switch (msg.cmd) {
    case APP_CMD_HMI_ACTION:
      uiBusHandleAkce(static_cast<UiAkceTlacitko>(msg.arg));
      break;
    case APP_CMD_POWER_START:
      ESP_LOGI(TAG, "queue → START (src=%s)", spSrcName(msg.src));
      provedStart();
      break;
    case APP_CMD_POWER_STOP:
      ESP_LOGI(TAG, "queue → STOP (src=%s)", spSrcName(msg.src));
      provedStop();
      break;
    case APP_CMD_SETPOINT_ABS:
      if (autoMode) {
        provedRoomSpAbs((float)msg.arg / 10.0f, msg.src);
      } else {
        provedTeplotaAbsolutni((uint8_t)msg.arg, msg.src);
      }
      break;
    case APP_CMD_SETPOINT_DELTA:
      if (autoMode) {
        provedRoomSpZmena((float)msg.arg / 10.0f, msg.src);
      } else {
        provedTeplotaZmena(msg.arg, msg.src);
      }
      break;
    case APP_CMD_SET_MODE:
      uiBusSetRegulationAuto(msg.arg != 0);
      ESP_LOGI(TAG, "queue → mode %s (src=%s)",
               msg.arg ? "room" : "water", spSrcName(msg.src));
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
      if (uiEez.rezim == UI_REZIM_AUTO) {
        provedRoomSpZmena(0.5f, UI_SP_SRC_HMI);
      } else {
        provedTeplotaZmena(1, UI_SP_SRC_HMI);
      }
      break;
    case UI_AKCE_TEPLOTA_MINUS:
      if (uiEez.rezim == UI_REZIM_AUTO) {
        provedRoomSpZmena(-0.5f, UI_SP_SRC_HMI);
      } else {
        provedTeplotaZmena(-1, UI_SP_SRC_HMI);
      }
      break;
    case UI_AKCE_REZIM_PREPNOUT:
      if (uiEez.rezim == UI_REZIM_AUTO) {
        uiEez.rezim = UI_REZIM_VYSTUPNI_TEPLOTA;
        // Session SP = aktuální A0
        lgModelLock();
        {
          const uint8_t a0Sp = lgModelA0Bajt(8);
          if (a0Sp >= 15 && a0Sp <= 65) {
            mCilova = a0Sp;
            if (drzetStavAktivni) {
              cilovaTeplotaTab5 = a0Sp;
            }
          }
        }
        lgModelUnlock();
      } else {
        uiEez.rezim = UI_REZIM_AUTO;
        if (drzenyZapnuty()) {
          RegulatorSnapshot snap{};
          climateRegulatorGetSnapshot(&snap);
          provedTeplotaAbsolutni(snap.t_water_c, UI_SP_SRC_REGULATOR);
        } else {
          ESP_LOGI(TAG, "Auto zapamatovan — ceka START (cerpadlo vyp)");
        }
      }
      uiBusPersistRezim();
      ESP_LOGI(TAG, "rezim -> %s",
               uiEez.rezim == UI_REZIM_AUTO ? "AUTO" : "VYSTUPNI");
      break;
    default:
      break;
  }
}

void uiBusSetWaterSp(uint8_t teplotaC, UiSpSource src) {
  provedTeplotaAbsolutni(teplotaC, src);
}

void uiBusSetSetpointC(uint8_t teplotaC) {
  provedTeplotaAbsolutni(teplotaC, UI_SP_SRC_REGULATOR);
}

void uiBusAdjustSetpoint(int deltaC) {
  if (uiEez.rezim == UI_REZIM_AUTO) {
    provedRoomSpZmena((float)deltaC / 10.0f, UI_SP_SRC_HMI);
  } else {
    provedTeplotaZmena(deltaC, UI_SP_SRC_HMI);
  }
}

void uiBusQueuePower(bool start) {
  appCmdEnqueuePower(start, UI_SP_SRC_MQTT);
}

void uiBusQueueSetpointC(uint8_t teplotaC) {
  appCmdEnqueueSetpointAbs((int)teplotaC, UI_SP_SRC_MQTT);
}

void uiBusQueueAdjustSetpoint(int deltaC) {
  appCmdEnqueueAdjust(deltaC, UI_SP_SRC_MQTT);
}

void uiBusQueueSetRegulationAuto(bool roomMode) {
  appCmdEnqueueMode(roomMode, UI_SP_SRC_MQTT);
}

void uiBusPlanApplyStart(void) {
  provedStart();
}

void uiBusPlanApplyStop(void) {
  provedStop();
}

void uiBusPlanApplySetpoint(uint8_t teplotaC) {
  // Auto: plán nesmí měnit SP vody (jen offset/VYP přes climate_plan)
  provedTeplotaAbsolutni(teplotaC, UI_SP_SRC_PLAN);
}

bool uiBusSetRegulationAuto(bool enable) {
  uiEez.rezim = enable ? UI_REZIM_AUTO : UI_REZIM_VYSTUPNI_TEPLOTA;
  uiBusPersistRezim();
  return true;
}

void uiBusPersistRezim(void) {
  storageSaveUiRezim((uint8_t)uiEez.rezim);
}

void uiBusBindingsTick(void) {
  appCmdDrainCtrl();
  climatePlanTick();
  climateRegulatorTick();
  uiEezSyncFromBus();
}

void uiBusFlushDeferredStorage(void) {
  static uint32_t s_lastFlushMs = 0;
  const uint32_t now = millis();
  if (s_lastFlushMs != 0 && (now - s_lastFlushMs) < 400) {
    return;
  }
  s_lastFlushMs = now;
  storageFlushTcSessionPending();
  climateRegulatorFlushPendingSave();
}

void uiBusProcessAppMsg(const AppMsg* msg) {
  if (!msg) {
    return;
  }
  processAppMsg(*msg);
}
