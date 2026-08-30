// displej.h — stabilni verze pro Tab5 (bez tezkeho UTF-8 kresleni)
#ifndef DISPLEJ_H
#define DISPLEJ_H

#include "M5Unified.h"
#include "ui_text_ui.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "bus_lg_lin_api.h"
#define DOTYK_DEBOUNCE_MS 250
#define DISPLEJ_A0_INTERVAL_MS 3000

#define BTN_MINUS_X 40
#define BTN_MINUS_Y 380
#define BTN_MINUS_W 105
#define BTN_MINUS_H 70

#define BTN_PLUS_X 155
#define BTN_PLUS_Y 380
#define BTN_PLUS_W 105
#define BTN_PLUS_H 70

#define BTN_POWER_X 40
#define BTN_POWER_Y 470
#define BTN_POWER_W 220
#define BTN_POWER_H 90

#define BTN_SOLO_X 470
#define BTN_SOLO_Y 8
#define BTN_SOLO_W 200
#define BTN_SOLO_H 64

#define BTN_DRZET_X 680
#define BTN_DRZET_Y 8
#define BTN_DRZET_W 200
#define BTN_DRZET_H 64

#define BTN_LOG_X 890
#define BTN_LOG_Y 8
#define BTN_LOG_W 200
#define BTN_LOG_H 64

static bool displejStatickeOk = false;
static unsigned long displejPosledniObnovaMs = 0;
static char displejCisloBuf[16];

static int8_t displejPredStart = -1;
static int8_t displejPredKomp = -1;
static int8_t displejPredCerpadlo = -1;
static int8_t displejPredPatrona = -1;
static int8_t displejPredDefrost = -1;
static int8_t displejPredPower = -1;
static int8_t displejPredCekameOrig = -1;
static int8_t displejPredTcUi = -1;
static uint8_t displejPredCilova = 255;
static uint8_t displejPredVstupni = 255;
static uint8_t displejPredVystupni = 255;
static uint8_t displejPredDelta = 255;

enum DotykBtn : int8_t {
  DOTYK_ZADNY = -1,
  DOTYK_LOG,
  DOTYK_SOLO,
  DOTYK_DRZET,
  DOTYK_MINUS,
  DOTYK_PLUS,
  DOTYK_POWER
};

inline bool jeVTlacitku(int tx, int ty, int x, int y, int w, int h) {
  return x <= tx && tx < (x + w) && y <= ty && ty < (y + h);
}

inline uint8_t zobrazovanaCilova() {
  return pozadavekNaZapis ? novaCilovaTeplota : mCilova;
}

inline void displejTextStred(int x, int y, int w, const char* text, uint16_t barva) {
  M5.Display.setTextColor(barva);
  M5.Display.setFont(&fonts::Font4);
  int tw = M5.Display.textWidth(text);
  M5.Display.drawString(text, x + (w - tw) / 2, y);
}

inline void displejCislo(int x, int y, int w, uint16_t pozadi, uint16_t barva, uint8_t hodnota) {
  M5.Display.fillRect(x, y, w, 56, pozadi);
  M5.Display.setTextColor(barva);
  M5.Display.setFont(&fonts::Font4);
  snprintf(displejCisloBuf, sizeof(displejCisloBuf), "%u", hodnota);
  displejTextStred(x, y + 8, w, displejCisloBuf, barva);
}

inline void displejDesetina(int x, int y, int w, uint16_t pozadi, uint16_t barva, uint8_t desetiny) {
  M5.Display.fillRect(x, y, w, 56, pozadi);
  M5.Display.setTextColor(barva);
  M5.Display.setFont(&fonts::Font4);
  snprintf(displejCisloBuf, sizeof(displejCisloBuf), "%.1f", desetiny / 10.0);
  displejTextStred(x, y + 8, w, displejCisloBuf, barva);
}

inline void displejObnovStavText(const char* zdroj) {
  M5.Display.fillRect(0, 0, 420, 80, BLACK);
  M5.Display.setTextColor(LIGHTGREY);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setCursor(40, 24);
  M5.Display.print(zdroj != nullptr ? zdroj : "");
}

inline void displejKontrolku(int x, int y, int w, int h, const char* text, bool aktivni, uint16_t barvaAkt) {
  uint16_t pozadi = aktivni ? barvaAkt : M5.Display.color565(40, 40, 40);
  M5.Display.fillRect(x, y, w, h, pozadi);
  M5.Display.drawRect(x, y, w, h, WHITE);
  displejTextStred(x, y + 28, w, text, aktivni ? BLACK : WHITE);
}

void displejObnovPowerTlacitko() {
  uint8_t b2 = lgModelA0Bajt(2);
  uint8_t b3 = lgModelA0Bajt(3);
  bool tcBezi = lgJeTcProvoz(b2, b3);
  uint16_t pwrBarva;
  const char* pwrText;
  if (tcBezi) {
    pwrBarva = RED;
    pwrText = TXT_VYPNOUT_TC;
  } else if (cekameNaOrigStart) {
    pwrBarva = YELLOW;
    pwrText = TXT_STORNO_CEKANI;
  } else {
    pwrBarva = GREEN;
    pwrText = TXT_ZAPNOUT_TC;
  }
  M5.Display.fillRect(BTN_POWER_X, BTN_POWER_Y, BTN_POWER_W, BTN_POWER_H, pwrBarva);
  M5.Display.drawRect(BTN_POWER_X, BTN_POWER_Y, BTN_POWER_W, BTN_POWER_H, WHITE);
  displejTextStred(BTN_POWER_X, BTN_POWER_Y + 28, BTN_POWER_W, pwrText, BLACK);
}

void displejNakresliStaticke() {
  M5.Display.fillScreen(BLACK);

  M5.Display.fillRect(40, 100, 220, 260, CYAN);
  M5.Display.setTextColor(BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.drawString(TXT_CILOVA_VODA, 60, 120);

  M5.Display.fillRect(280, 100, 220, 260, BLUE);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString(TXT_VSTUPNI_VODA, 300, 120);

  M5.Display.fillRect(520, 100, 220, 260, RED);
  M5.Display.drawString(TXT_VYSTUPNI_VODA, 540, 120);

  M5.Display.fillRect(280, 560, 960, 120, NAVY);
  M5.Display.setTextColor(WHITE);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.drawString(TXT_TEPL_SPAD, 320, 600);

  M5.Display.fillRect(BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H, DARKGREY);
  M5.Display.drawRect(BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H, WHITE);
  M5.Display.drawString("-1 C", BTN_MINUS_X + 25, BTN_MINUS_Y + 22);

  M5.Display.fillRect(BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H, DARKGREY);
  M5.Display.drawRect(BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H, WHITE);
  M5.Display.drawString("+1 C", BTN_PLUS_X + 22, BTN_PLUS_Y + 22);

  displejObnovPowerTlacitko();
  displejPredPower = stavZapnuto;
  displejStatickeOk = true;
}

void displejObnovHorniTlacitka() {
  uint16_t logBarva = monitorPozastaven ? ORANGE : DARKGREEN;
  M5.Display.fillRect(BTN_LOG_X, BTN_LOG_Y, BTN_LOG_W, BTN_LOG_H, logBarva);
  M5.Display.drawRect(BTN_LOG_X, BTN_LOG_Y, BTN_LOG_W, BTN_LOG_H, WHITE);
  displejTextStred(BTN_LOG_X, BTN_LOG_Y + 18, BTN_LOG_W,
                     monitorPozastaven ? TXT_LOG_PAUZA : TXT_LOG_BEZI, WHITE);

  uint16_t soloBarva = soloRezimTab5 ? CYAN : DARKGREY;
  M5.Display.fillRect(BTN_SOLO_X, BTN_SOLO_Y, BTN_SOLO_W, BTN_SOLO_H, soloBarva);
  M5.Display.drawRect(BTN_SOLO_X, BTN_SOLO_Y, BTN_SOLO_W, BTN_SOLO_H, WHITE);
  displejTextStred(BTN_SOLO_X, BTN_SOLO_Y + 18, BTN_SOLO_W,
                     soloRezimTab5 ? TXT_SOLO_OVLADAM : TXT_SOLO_VYP,
                     soloRezimTab5 ? BLACK : WHITE);

  uint16_t drzBarva = drzetStavProtiOrig ? YELLOW : DARKGREY;
  M5.Display.fillRect(BTN_DRZET_X, BTN_DRZET_Y, BTN_DRZET_W, BTN_DRZET_H, drzBarva);
  M5.Display.drawRect(BTN_DRZET_X, BTN_DRZET_Y, BTN_DRZET_W, BTN_DRZET_H, WHITE);
  displejTextStred(BTN_DRZET_X, BTN_DRZET_Y + 18, BTN_DRZET_W,
                     drzetStavProtiOrig ? TXT_DRZET_ZAP : TXT_DRZET_VYP,
                     drzetStavProtiOrig ? BLACK : WHITE);
}

void displejObnovHorniPanel(const char* zdroj) {
  M5.Display.fillRect(0, 0, 420, 80, BLACK);
  M5.Display.setTextColor(LIGHTGREY);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setCursor(40, 24);
  M5.Display.print(zdroj != nullptr ? zdroj : "");
  displejObnovHorniTlacitka();
}

void displejLehkaObnovaPoDotyku(bool powerBtn) {
  displejPosledniObnovaMs = millis();
  if (powerBtn) {
    displejPredTcUi = -1;
    displejPredStart = -1;
    displejObnovStavText(posledniStavovyText);
    displejObnovPowerTlacitko();
  } else {
    displejObnovHorniTlacitka();
  }
}

void displejObnovData() {
  uint8_t cilova = zobrazovanaCilova();
  if (cilova != displejPredCilova) {
    displejCislo(40, 170, 200, CYAN, BLACK, cilova);
    displejPredCilova = cilova;
  }
  yield();

  if (mVstupni != displejPredVstupni) {
    displejCislo(280, 170, 200, BLUE, WHITE, mVstupni);
    displejPredVstupni = mVstupni;
  }
  yield();

  if (mVystupni != displejPredVystupni) {
    displejCislo(520, 170, 200, RED, WHITE, mVystupni);
    displejPredVystupni = mVystupni;
  }
  yield();

  int kY = 430;
  uint8_t b2 = lgModelA0Bajt(2);
  uint8_t b3 = lgModelA0Bajt(3);
  bool isStart     = lgJeTcProvoz(b2, b3) || tcPozadavekZap || cekameNaOrigStart;
  bool isKompresor = lgJeStabilniBeh(b3);
  bool isCerpadlo  = lgJeCerpadloZap(b2);
  bool isPatrona   = (lgModelA0Bajt(2) & 0x04);
  bool isDefrost   = (lgModelA0Bajt(3) & 0x04);

  if (isStart != displejPredStart) {
    displejKontrolku(280, kY, 180, 90, TXT_TC_ZAPNUTO, isStart, GREEN);
    displejPredStart = isStart;
  }
  if (isKompresor != displejPredKomp) {
    displejKontrolku(480, kY, 180, 90, "KOMPRESOR", isKompresor, RED);
    displejPredKomp = isKompresor;
  }
  if (isCerpadlo != displejPredCerpadlo) {
    displejKontrolku(680, kY, 180, 90, TXT_OBEH_CERPADLO, isCerpadlo, GREEN);
    displejPredCerpadlo = isCerpadlo;
  }
  if (isPatrona != displejPredPatrona) {
    displejKontrolku(880, kY, 180, 90, "EL. PATRONA", isPatrona, ORANGE);
    displejPredPatrona = isPatrona;
  }
  if (isDefrost != displejPredDefrost) {
    displejKontrolku(1080, kY, 180, 90, TXT_ODMRAZOVANI, isDefrost, YELLOW);
    displejPredDefrost = isDefrost;
  }
  yield();

  uint8_t deltaT = (uint8_t)abs((int)mVystupni - (int)mVstupni);
  if (deltaT != displejPredDelta) {
    displejCislo(800, 565, 120, NAVY, WHITE, deltaT);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextColor(WHITE);
    M5.Display.drawString("st C", 930, 590);
    displejPredDelta = deltaT;
  }

  int8_t tcUi = (cekameNaOrigStart && !lgJeTcProvoz(b2, b3)) ? 2
              : (lgJeTcZapnutoProUi(b2, b3) ? 1 : 0);
  if (tcUi != displejPredTcUi) {
    displejObnovPowerTlacitko();
    displejPredTcUi = tcUi;
    displejPredPower = stavZapnuto;
    displejPredCekameOrig = cekameNaOrigStart;
  }
}

inline bool displejJeCasNaObnovu(bool okamzite = false) {
  return okamzite
      || (millis() - displejPosledniObnovaMs) >= DISPLEJ_A0_INTERVAL_MS
      || displejPredVstupni == 255;
}

void aktualizujDisplej(const char* zdroj, bool okamzite = false) {
  if (!displejJeCasNaObnovu(okamzite)) {
    return;
  }
  displejPosledniObnovaMs = millis();

  if (!displejStatickeOk) {
    displejNakresliStaticke();
  }

  if (okamzite) {
    displejObnovHorniPanel(zdroj);
  } else {
    displejObnovStavText(zdroj);
  }
  yield();

  if (lgModelA0Bajt(0, 0) == 0xA0) {
    displejObnovData();
  }
}

inline void obnovDisplej(bool okamzite = false) {
  aktualizujDisplej(posledniStavovyText, okamzite);
}

inline DotykBtn lgZjistiTlacitko(int tx, int ty) {
  if (jeVTlacitku(tx, ty, BTN_LOG_X, BTN_LOG_Y, BTN_LOG_W, BTN_LOG_H)) { return DOTYK_LOG; }
  if (jeVTlacitku(tx, ty, BTN_SOLO_X, BTN_SOLO_Y, BTN_SOLO_W, BTN_SOLO_H)) { return DOTYK_SOLO; }
  if (jeVTlacitku(tx, ty, BTN_DRZET_X, BTN_DRZET_Y, BTN_DRZET_W, BTN_DRZET_H)) { return DOTYK_DRZET; }
  if (jeVTlacitku(tx, ty, BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H)) { return DOTYK_MINUS; }
  if (jeVTlacitku(tx, ty, BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H)) { return DOTYK_PLUS; }
  if (jeVTlacitku(tx, ty, BTN_POWER_X, BTN_POWER_Y, BTN_POWER_W, BTN_POWER_H)) { return DOTYK_POWER; }
  return DOTYK_ZADNY;
}

inline bool lgJeVTlacitku(DotykBtn btn, int tx, int ty) {
  switch (btn) {
    case DOTYK_LOG:   return jeVTlacitku(tx, ty, BTN_LOG_X, BTN_LOG_Y, BTN_LOG_W, BTN_LOG_H);
    case DOTYK_SOLO:  return jeVTlacitku(tx, ty, BTN_SOLO_X, BTN_SOLO_Y, BTN_SOLO_W, BTN_SOLO_H);
    case DOTYK_DRZET: return jeVTlacitku(tx, ty, BTN_DRZET_X, BTN_DRZET_Y, BTN_DRZET_W, BTN_DRZET_H);
    case DOTYK_MINUS: return jeVTlacitku(tx, ty, BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H);
    case DOTYK_PLUS:  return jeVTlacitku(tx, ty, BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H);
    case DOTYK_POWER: return jeVTlacitku(tx, ty, BTN_POWER_X, BTN_POWER_Y, BTN_POWER_W, BTN_POWER_H);
    default: return false;
  }
}

inline bool zpracujDotykTab5() {
  static DotykBtn dotykBtnStart = DOTYK_ZADNY;
  static unsigned long dotykPosledniAkceMs = 0;

  if (M5.Touch.getCount() == 0) { return false; }

  auto detail = M5.Touch.getDetail(0);

  if (detail.wasPressed()) {
    dotykBtnStart = lgZjistiTlacitko(detail.x, detail.y);
    return false;
  }
  if (detail.wasHold() || detail.isHolding()) {
    dotykBtnStart = DOTYK_ZADNY;
    return false;
  }
  if (!detail.wasClicked()) { return false; }

  DotykBtn btn = dotykBtnStart;
  if (btn == DOTYK_ZADNY) {
    btn = lgZjistiTlacitko(detail.base_x, detail.base_y);
  }
  dotykBtnStart = DOTYK_ZADNY;
  if (btn == DOTYK_ZADNY) { return false; }
  if (millis() - dotykPosledniAkceMs < DOTYK_DEBOUNCE_MS) { return false; }
  if (!lgJeVTlacitku(btn, detail.base_x, detail.base_y)) { return false; }
  if (abs(detail.x - detail.base_x) > 35 || abs(detail.y - detail.base_y) > 35) { return false; }

  dotykPosledniAkceMs = millis();
  uint8_t aktualniCilova = pozadavekNaZapis ? novaCilovaTeplota : mCilova;
  bool dotyk = false;

  switch (btn) {
    case DOTYK_LOG:
      prepniMonitorPozastaven();
      displejLehkaObnovaPoDotyku(false);
      return true;
    case DOTYK_SOLO:
      prepniSoloRezim();
      displejLehkaObnovaPoDotyku(false);
      return true;
    case DOTYK_DRZET:
      prepniDrzetStav();
      displejLehkaObnovaPoDotyku(false);
      return true;
    case DOTYK_MINUS:
      if (aktualniCilova > 15) {
        novaCilovaTeplota = aktualniCilova - 1;
        mCilova = novaCilovaTeplota;
        pozadavekNaZapis = true;
        pozadavekZmenaStartu = false;
        dotyk = true;
      }
      break;
    case DOTYK_PLUS:
      if (aktualniCilova < 65) {
        novaCilovaTeplota = aktualniCilova + 1;
        mCilova = novaCilovaTeplota;
        pozadavekNaZapis = true;
        pozadavekZmenaStartu = false;
        dotyk = true;
      }
      break;
    case DOTYK_POWER: {
      uint8_t b2 = lgModelA0Bajt(2);
      uint8_t b3 = lgModelA0Bajt(3);
      bool tcBezi = lgJeTcProvoz(b2, b3);
      bool zapProZapis = true;

      if (tcBezi) {
        if (lgZapisBezi()) {
          Serial.println("[DOTYK Tab5] STOP ignorovan — probiha sekvence");
          dotyk = true;
          break;
        }
        if (cekameNaOrigStart) {
          lgUkonciCekaniProStop();
        }
        tcPozadavekZap = false;
        stavZapnuto = false;
        novaCilovaTeplota = aktualniCilova;
        mCilova = aktualniCilova;
        lgNastavDrzenyStav(aktualniCilova, false);
        zapProZapis = false;
        Serial.printf("[DOTYK Tab5] STOP -> VYP (SOLO=%s)\n", soloRezimTab5 ? "ano" : "ne");
      } else if (cekameNaOrigStart) {
        lgZrusCekaniOrig("storno z Tab5");
        nastavStavovyText(TXT_STAV_CEKAM);
        displejLehkaObnovaPoDotyku(true);
        Serial.println("[DOTYK Tab5] STORNO — cekani orig. zruseno (bez STOP sekvence)");
        return true;
      } else if (soloRezimTab5) {
        stavZapnuto = true;
        tcPozadavekZap = true;
        novaCilovaTeplota = aktualniCilova;
        mCilova = aktualniCilova;
        lgNastavDrzenyStav(aktualniCilova, true);
        zapProZapis = true;
        Serial.println("[DOTYK Tab5] START -> ZAP (SOLO, LIN sekvence)");
      } else {
        novaCilovaTeplota = aktualniCilova;
        mCilova = aktualniCilova;
        tcPozadavekZap = true;
        lgNastavDrzenyStav(aktualniCilova, true);
        zapProZapis = true;
        nastavStavovyText(TXT_STAV_CEKAM_ORIG);
        Serial.println("[DOTYK Tab5] START -> cekame orig. ovladac");
      }
      provedZapisTeploty(novaCilovaTeplota, zapProZapis, true);
      displejLehkaObnovaPoDotyku(true);
      dotyk = true;
      break;
    }
    default: break;
  }

  if (dotyk) {
    if (!cekameNaOrigStart) {
      nastavStavovyText(TXT_DOTYK_CEKAM);
    }
    return true;
  }
  return false;
}

#endif
