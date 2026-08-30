// ui_displej_hmi.h — HMI 1280x720 z HTML mockupu (gen_hmi_eez_project.py)
// Kresleni prime na M5.Display z loop() core 1 (bez PSRAM canvas).
#ifndef UI_DISPLEJ_HMI_H
#define UI_DISPLEJ_HMI_H

#include "M5Unified.h"
#include "ui_eez_model.h"
#include "ui_hmi_layout.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "bus_lg_lin_api.h"

#define HMI_REFRESH_MS 500

#define HMI_COL_BG       0x121214u
#define HMI_COL_PANEL    0x1A1A1Fu
#define HMI_COL_BORDER   0x24242Bu
#define HMI_COL_GREEN    0x30D158u
#define HMI_COL_ORANGE   0xFF9F0Au
#define HMI_COL_BLUE     0x0A84FFu
#define HMI_COL_CYAN     0x64D2FFu
#define HMI_COL_GREY     0x8E8E93u
#define HMI_COL_GREY2    0xAEAEAEu
#define HMI_COL_BTN      0x24242Bu
#define HMI_COL_BTN_BD   0x3A3A45u
#define HMI_COL_STOP_TX  0x636366u
#define HMI_COL_TECH_BG  0x151518u
#define HMI_COL_LED_OFF  0x2C2C2Eu

#define HMI_SETPOINT_X 280
#define HMI_SETPOINT_Y 165
#define HMI_SETPOINT_W 200
#define HMI_SETPOINT_H 56

static bool hmiStaticOk = false;
static unsigned long hmiLastRefreshMs = 0;
static char hmiBuf[96];

static float hmiPredSetpoint = -1000.0f;
static float hmiPredVstup = -1000.0f;
static float hmiPredVystup = -1000.0f;
static float hmiPredVnitrni = -1000.0f;
static float hmiPredVenkovni = -1000.0f;
static bool hmiPredSig[5] = {false, false, false, false, false};
static bool hmiPredUtlum = false;
static bool hmiPredWifi = false;
static bool hmiPredMqtt = false;
static char hmiPredPlan[96] = "";
static char hmiPredCas[8] = "";

#define HMI_D M5.Display

inline uint16_t hmiRgb(uint32_t rgb) {
  return HMI_D.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

inline void hmiTextCenter(int x, int y, int w, const char* text, uint16_t color,
                          const lgfx::IFont* font = &fonts::Font2) {
  HMI_D.setFont(font);
  HMI_D.setTextColor(color);
  const int tw = HMI_D.textWidth(text);
  HMI_D.drawString(text, x + (w - tw) / 2, y);
}

inline void hmiFillRoundBtn(int x, int y, int w, int h, uint32_t bg, uint32_t border,
                            const char* label, uint32_t textCol, int radius = 8) {
  HMI_D.fillRoundRect(x, y, w, h, radius, hmiRgb(bg));
  if (border) {
    HMI_D.drawRoundRect(x, y, w, h, radius, hmiRgb(border));
  }
  hmiTextCenter(x, y + (h - 16) / 2, w, label, hmiRgb(textCol),
                (h == 100) ? &fonts::Font4 : &fonts::Font2);
}

inline void hmiFormatTeplota(char* buf, size_t len, float c) {
  if (c <= -999.0f) {
    snprintf(buf, len, "---");
    return;
  }
  snprintf(buf, len, "%.1f", c);
}

// Jeden krok statickeho panelu — vraci true = jeste nejsou dalsi kroky.
inline bool hmiNakresliStaticKrok(int step) {
  auto& d = HMI_D;
  switch (step) {
    case 0:
      d.fillScreen(hmiRgb(HMI_COL_BG));
      d.fillRect(0, 0, 1280, 60, hmiRgb(HMI_COL_PANEL));
      d.drawFastHLine(0, 59, 1280, hmiRgb(HMI_COL_BORDER));
      return true;
    case 1:
      d.fillRect(0, 600, 1280, 120, hmiRgb(HMI_COL_PANEL));
      d.drawFastHLine(0, 600, 1280, hmiRgb(HMI_COL_BORDER));
      d.fillRect(850, 60, 430, 540, hmiRgb(HMI_COL_TECH_BG));
      d.drawFastVLine(850, 60, 540, hmiRgb(HMI_COL_BORDER));
      d.drawFastVLine(203, 600, 120, hmiRgb(HMI_COL_BORDER));
      d.drawFastVLine(421, 600, 120, hmiRgb(HMI_COL_BORDER));
      d.drawFastVLine(719, 600, 120, hmiRgb(HMI_COL_BORDER));
      d.fillRoundRect(40, 430, 770, 90, 8, hmiRgb(HMI_COL_PANEL));
      d.drawRoundRect(40, 430, 770, 90, 8, hmiRgb(HMI_COL_BORDER));
      return true;
    case 2:
      d.setFont(&fonts::Font2);
      d.setTextColor(hmiRgb(HMI_COL_GREY));
      d.drawString("POZADOVANA TEPLOTA", 400, 130);
      d.setTextColor(hmiRgb(0xFFFFFF));
      d.setFont(&fonts::Font4);
      d.drawString("C", 590, 200);
      d.setFont(&fonts::Font2);
      d.setTextColor(hmiRgb(HMI_COL_ORANGE));
      d.drawString("TYDENNI PLAN AKTIVNI", 60, 445);
      d.setTextColor(hmiRgb(HMI_COL_GREY));
      d.drawString("Vnitrni teplota", 25, 620);
      d.drawString("Venkovni teplota", 243, 620);
      d.drawString("Vstupni voda", 524, 620);
      d.drawString("Vystupni voda", 806, 620);
      return true;
    case 3: {
      const char* indLabels[] = {
          "BEH SYSTEMU", "OBEHOVE CERPADLO", "KOMPRESOR", "ODMRAZOVANI", "PRIDAVNE EL. TOPENI"};
      d.setFont(&fonts::Font2);
      for (int i = 0; i < 5; ++i) {
        d.setTextColor(hmiRgb(0xE0E0E6));
        d.drawString(indLabels[i], 901, 179 + i * 42);
      }
      return true;
    }
    case 4:
      hmiFillRoundBtn(BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H, HMI_COL_BTN,
                      HMI_COL_BTN_BD, "-", 0xFFFFFF, 40);
      hmiFillRoundBtn(BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H, HMI_COL_BTN,
                      HMI_COL_BTN_BD, "+", 0xFFFFFF, 40);
      hmiFillRoundBtn(BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H, HMI_COL_GREEN, 0, "RUN",
                      0x000000, 8);
      hmiFillRoundBtn(BTN_STOP_X, BTN_STOP_Y, BTN_STOP_W, BTN_STOP_H, HMI_COL_BTN, 0, "STOP",
                      HMI_COL_STOP_TX, 8);
      hmiFillRoundBtn(BTN_MENU_X, BTN_MENU_Y, BTN_MENU_W, BTN_MENU_H, HMI_COL_BTN, 0, "MENU",
                      0xFFFFFF, 0);
      hmiFillRoundBtn(BTN_TICHY_X, BTN_TICHY_Y, BTN_TICHY_W, BTN_TICHY_H, HMI_COL_BLUE, 0,
                      "Tichy rezim", 0xFFFFFF, 6);
      hmiStaticOk = true;
      return false;
    default:
      hmiStaticOk = true;
      return false;
  }
}

inline void hmiNakresliPanelStaticke() {
  for (int step = 0; hmiNakresliStaticKrok(step); ++step) {
  }
}

inline void hmiNakresliIndikator(int row, bool aktivni, uint32_t barvaOn) {
  const int y = 185 + row * 42;
  const uint32_t col = aktivni ? barvaOn : HMI_COL_LED_OFF;
  HMI_D.fillCircle(877, y + 7, 7, hmiRgb(col));
}

inline void hmiNakresliSetpoint(float teplota, const lgfx::IFont* font = &fonts::Font6) {
  HMI_D.fillRect(HMI_SETPOINT_X, HMI_SETPOINT_Y, HMI_SETPOINT_W, HMI_SETPOINT_H,
                 hmiRgb(HMI_COL_BG));
  hmiFormatTeplota(hmiBuf, sizeof(hmiBuf), teplota);
  hmiTextCenter(HMI_SETPOINT_X, HMI_SETPOINT_Y + 8, HMI_SETPOINT_W, hmiBuf,
                hmiRgb(0xFFFFFF), font);
}

inline void hmiNakresliTeplotu(int x, int y, float teplota, uint32_t barva) {
  HMI_D.fillRect(x, y, 180, 40, hmiRgb(HMI_COL_PANEL));
  hmiFormatTeplota(hmiBuf, sizeof(hmiBuf), teplota);
  HMI_D.setFont(&fonts::Font4);
  HMI_D.setTextColor(hmiRgb(barva));
  HMI_D.drawString(hmiBuf, x, y);
}

inline void hmiNakresliTichyBtn() {
  const uint32_t bg = uiEez.sig_utlum ? HMI_COL_ORANGE : HMI_COL_BLUE;
  hmiFillRoundBtn(BTN_TICHY_X, BTN_TICHY_Y, BTN_TICHY_W, BTN_TICHY_H, bg, 0, "Tichy rezim",
                  0xFFFFFF, 6);
}

// Boot: jeden dynamicke prvek na tick (snizuje brownout).
inline bool hmiNakresliDynamicKrok(int step) {
  if (!hmiStaticOk) {
    return false;
  }

  uiEezSyncFromBus();

  switch (step) {
    case 0:
      hmiNakresliSetpoint(uiEez.teplota_vody_set, &fonts::Font4);
      hmiPredSetpoint = uiEez.teplota_vody_set;
      return true;
    case 1:
      hmiNakresliTeplotu(50, 645, uiEez.teplota_vnitrni, 0xFFFFFF);
      hmiPredVnitrni = uiEez.teplota_vnitrni;
      return true;
    case 2:
      hmiNakresliTeplotu(281, 645, uiEez.teplota_venkovni, HMI_COL_CYAN);
      hmiPredVenkovni = uiEez.teplota_venkovni;
      return true;
    case 3:
      hmiNakresliTeplotu(545, 645, uiEez.teplota_vody_vstup, 0xFFFFFF);
      hmiPredVstup = uiEez.teplota_vody_vstup;
      return true;
    case 4:
      hmiNakresliTeplotu(823, 645, uiEez.teplota_vody_vystup, HMI_COL_ORANGE);
      hmiPredVystup = uiEez.teplota_vody_vystup;
      return true;
    case 5: {
      const bool sigs[5] = {uiEez.sig_chod, uiEez.sig_cerpadlo, uiEez.sig_kompresor,
                            uiEez.sig_odmrazovani, uiEez.sig_el_topeni};
      const uint32_t sigColors[5] = {HMI_COL_GREEN, HMI_COL_GREEN, HMI_COL_ORANGE,
                                     HMI_COL_GREEN, HMI_COL_ORANGE};
      for (int i = 0; i < 5; ++i) {
        hmiNakresliIndikator(i, sigs[i], sigColors[i]);
        hmiPredSig[i] = sigs[i];
      }
      return true;
    }
    case 6:
      HMI_D.fillRect(500, 15, 200, 24, hmiRgb(HMI_COL_PANEL));
      HMI_D.setFont(&fonts::Font2);
      HMI_D.setTextColor(hmiRgb(uiEez.sig_wifi ? HMI_COL_GREEN : HMI_COL_GREY));
      HMI_D.drawString(uiEez.sig_wifi ? "Wi-Fi: OK" : "Wi-Fi: ---", 500, 20);
      hmiPredWifi = uiEez.sig_wifi;
      return true;
    case 7:
      HMI_D.fillRect(720, 16, 280, 24, hmiRgb(HMI_COL_PANEL));
      HMI_D.setFont(&fonts::Font2);
      HMI_D.setTextColor(hmiRgb(uiEez.sig_mqtt ? HMI_COL_GREEN : HMI_COL_GREY));
      HMI_D.drawString(uiEez.sig_mqtt ? "MQTT: Pripojeno" : "MQTT: ---", 720, 20);
      hmiPredMqtt = uiEez.sig_mqtt;
      return true;
    case 8:
      HMI_D.fillRect(30, 16, 120, 28, hmiRgb(HMI_COL_PANEL));
      HMI_D.setFont(&fonts::Font4);
      HMI_D.setTextColor(hmiRgb(0xFFFFFF));
      HMI_D.drawString(uiEez.cas_text, 30, 16);
      strncpy(hmiPredCas, uiEez.cas_text, sizeof(hmiPredCas));
      return true;
    case 9:
      HMI_D.fillRect(60, 472, 730, 24, hmiRgb(HMI_COL_PANEL));
      HMI_D.setFont(&fonts::Font2);
      HMI_D.setTextColor(hmiRgb(HMI_COL_GREY2));
      HMI_D.drawString(uiEez.plan_text, 60, 472);
      strncpy(hmiPredPlan, uiEez.plan_text, sizeof(hmiPredPlan));
      hmiNakresliTichyBtn();
      hmiPredUtlum = uiEez.sig_utlum;
      return false;
    default:
      return false;
  }
}

inline void hmiNakresliDynamicke() {
  if (!hmiStaticOk) {
    return;
  }

  if (uiEez.teplota_vody_set != hmiPredSetpoint) {
    hmiNakresliSetpoint(uiEez.teplota_vody_set);
    hmiPredSetpoint = uiEez.teplota_vody_set;
  }

  if (uiEez.teplota_vnitrni != hmiPredVnitrni) {
    hmiNakresliTeplotu(50, 645, uiEez.teplota_vnitrni, 0xFFFFFF);
    hmiPredVnitrni = uiEez.teplota_vnitrni;
  }
  if (uiEez.teplota_venkovni != hmiPredVenkovni) {
    hmiNakresliTeplotu(281, 645, uiEez.teplota_venkovni, HMI_COL_CYAN);
    hmiPredVenkovni = uiEez.teplota_venkovni;
  }
  if (uiEez.teplota_vody_vstup != hmiPredVstup) {
    hmiNakresliTeplotu(545, 645, uiEez.teplota_vody_vstup, 0xFFFFFF);
    hmiPredVstup = uiEez.teplota_vody_vstup;
  }
  if (uiEez.teplota_vody_vystup != hmiPredVystup) {
    hmiNakresliTeplotu(823, 645, uiEez.teplota_vody_vystup, HMI_COL_ORANGE);
    hmiPredVystup = uiEez.teplota_vody_vystup;
  }

  const bool sigs[5] = {uiEez.sig_chod, uiEez.sig_cerpadlo, uiEez.sig_kompresor,
                          uiEez.sig_odmrazovani, uiEez.sig_el_topeni};
  const uint32_t sigColors[5] = {HMI_COL_GREEN, HMI_COL_GREEN, HMI_COL_ORANGE, HMI_COL_GREEN,
                                 HMI_COL_ORANGE};
  for (int i = 0; i < 5; ++i) {
    if (sigs[i] != hmiPredSig[i]) {
      hmiNakresliIndikator(i, sigs[i], sigColors[i]);
      hmiPredSig[i] = sigs[i];
    }
  }

  if (uiEez.sig_utlum != hmiPredUtlum) {
    hmiNakresliTichyBtn();
    hmiPredUtlum = uiEez.sig_utlum;
  }

  if (uiEez.sig_wifi != hmiPredWifi) {
    HMI_D.fillRect(500, 15, 200, 24, hmiRgb(HMI_COL_PANEL));
    HMI_D.setFont(&fonts::Font2);
    HMI_D.setTextColor(hmiRgb(uiEez.sig_wifi ? HMI_COL_GREEN : HMI_COL_GREY));
    HMI_D.drawString(uiEez.sig_wifi ? "Wi-Fi: OK" : "Wi-Fi: ---", 500, 20);
    hmiPredWifi = uiEez.sig_wifi;
  }

  if (uiEez.sig_mqtt != hmiPredMqtt) {
    HMI_D.fillRect(720, 16, 280, 24, hmiRgb(HMI_COL_PANEL));
    HMI_D.setFont(&fonts::Font2);
    HMI_D.setTextColor(hmiRgb(uiEez.sig_mqtt ? HMI_COL_GREEN : HMI_COL_GREY));
    HMI_D.drawString(uiEez.sig_mqtt ? "MQTT: Pripojeno" : "MQTT: ---", 720, 20);
    hmiPredMqtt = uiEez.sig_mqtt;
  }

  if (strncmp(uiEez.cas_text, hmiPredCas, sizeof(hmiPredCas)) != 0) {
    HMI_D.fillRect(30, 16, 120, 28, hmiRgb(HMI_COL_PANEL));
    HMI_D.setFont(&fonts::Font4);
    HMI_D.setTextColor(hmiRgb(0xFFFFFF));
    HMI_D.drawString(uiEez.cas_text, 30, 16);
    strncpy(hmiPredCas, uiEez.cas_text, sizeof(hmiPredCas));
  }

  if (strncmp(uiEez.plan_text, hmiPredPlan, sizeof(hmiPredPlan)) != 0) {
    HMI_D.fillRect(60, 472, 730, 24, hmiRgb(HMI_COL_PANEL));
    HMI_D.setFont(&fonts::Font2);
    HMI_D.setTextColor(hmiRgb(HMI_COL_GREY2));
    HMI_D.drawString(uiEez.plan_text, 60, 472);
    strncpy(hmiPredPlan, uiEez.plan_text, sizeof(hmiPredPlan));
  }
}

inline bool hmiJeCasNaObnovu(bool okamzite = false) {
  return okamzite || potrebaObnovitDisplej
      || (millis() - hmiLastRefreshMs) >= HMI_REFRESH_MS || !hmiStaticOk;
}

inline void hmiMarkRefreshDone() {
  hmiLastRefreshMs = millis();
  potrebaObnovitDisplej = false;
}

inline void hmiDrawToCanvas(bool okamzite = false) {
  if (!hmiJeCasNaObnovu(okamzite)) {
    return;
  }
  hmiMarkRefreshDone();
  uiEezSyncFromBus();
  hmiNakresliDynamicke();
}

inline void hmiObnovDisplej(bool okamzite = false) {
  hmiDrawToCanvas(okamzite);
}

inline void displejLehkaObnovaPoDotyku(bool) {
  potrebaObnovitDisplej = true;
}

inline void obnovDisplej(bool okamzite = false) {
  hmiObnovDisplej(okamzite);
}

inline void displejNakresliStaticke() {
  hmiNakresliPanelStaticke();
}

inline void displejObnovHorniPanel(const char*) {
  potrebaObnovitDisplej = true;
}

inline bool displejJeCasNaObnovu(bool okamzite = false) {
  return hmiJeCasNaObnovu(okamzite);
}

inline void displejObnovPowerTlacitko() {
  potrebaObnovitDisplej = true;
}

#endif
