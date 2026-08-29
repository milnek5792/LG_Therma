// lg_lin.h - Komunikace a odposlech sběrnice LG Therma V
#ifndef LG_LIN_H
#define LG_LIN_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "bus_lg_config.h"
#include "bus_lg_model.h"
#include "bus_lg_protocol.h"
#include "storage_config_nvs.h"

#define TAB5_MBUS_TX_PIN LG_MBUS_TX_PIN
#define TAB5_MBUS_RX_PIN LG_MBUS_RX_PIN
// LG_BAUDRATE z bus_lg_config.h
#define LG_PAKET_LEN 20

#define LG_ODPOSLECH_BEZ_ZAPISU 1
// 1 = Tab5 neposle prikaz, pokud na sbernici slysi orig. ovladac (C0)
#define LG_BLOKOVAT_ZAPIS_S_ORIG 1
#define LG_AUTO_OBNOVA_MIN_MS 4500
#define LG_AUTO_OBNOVA_PO_ORIG_MS 1200
/** Session ON: jak dlouho musí být A0 úplně VYP (bez pumpy), než znovu pošleme START. */
#ifndef LG_SESSION_VYP_GRACE_MS
#define LG_SESSION_VYP_GRACE_MS 45000u
#endif
#define LG_ZAPIS_MEZI_KROKY_MS 450
#define LG_START_KROK1_MS 2500
#define LG_START_OPAK_MS 3500
#define LG_START_CEKA_A0_MIN_MS 5000
#define LG_START_CEKA_A0_B3_MS 7500
#define LG_START_CEKA_A0_MAX_MS 28000
#define LG_START_MAX_POKUSU 4
#define LG_START_PO_ZRUSENI_MS 2500
// 0 = vždy plný dump (HEX/decode). 1 = u stejného A0 jen krátký řádek.
// LG_KRATKY_LOG default v bus_lg_config.h
#ifndef LG_KRATKY_LOG
#define LG_KRATKY_LOG 1
#endif
#ifndef LG_KRATKY_FULL_EVERY_MS
#define LG_KRATKY_FULL_EVERY_MS 20000UL
#endif
#define LG_ORIG_CEKANI_MAX_MS 120000

static uint8_t predchoziA0[64];
static uint8_t predchoziA0Len = 0;
static uint8_t predchoziC0[64];
static uint8_t predchoziC0Len = 0;
static unsigned long pocetPaketu = 0;

static uint8_t posledniNasTx[LG_PAKET_LEN];
static unsigned long casPosledniNasTx = 0;
static bool mamePosledniNasTx = false;

struct LgZapisFronta {
  bool aktivni = false;
  /** true = START/STOP sekvence (PRESTART); false = jen zmena teploty (+/-) */
  bool jeStartSekvence = false;
  uint8_t krok = 0;
  uint8_t opakovani = 0;
  uint8_t pocetKroku = 0;
  unsigned long casDalsiho = 0;
  bool cekaA0Start = false;
  unsigned long casCekaniStartOd = 0;
  bool videnPrestartB3 = false;
  uint8_t startPokus = 0;
  uint8_t startTeplota = 40;
  uint8_t pakety[4][LG_PAKET_LEN];
};

static LgZapisFronta lgZapis;
static unsigned long casPosledniAutoObnova = 0;
static unsigned long casPosledniC0Orig = 0;
static unsigned long casPosledniStartTx = 0;
static unsigned long casCekaniOrigOdMs = 0;
static bool origPoslalStartPripravu = false;
static uint8_t a0PosledniB2 = 0xFF;
static uint8_t a0PosledniB3 = 0xFF;
static uint8_t lgPosledniA0B2 = 0;
static uint8_t lgPosledniA0B3 = 0;

inline bool jeMonitorPozastaven() { return monitorPozastaven; }
static inline bool lgZapisAktivni() { return lgZapis.aktivni; }

inline bool lgSmiPosilatTx() {
#if LG_ODPOSLECH_BEZ_ZAPISU
  if (soloRezimTab5) { return true; }
  if (drzetStavAktivni) { return true; }
  return false;
#else
  return true;
#endif
}

inline void lgAktualizujPosledniA0(uint8_t b2, uint8_t b3) {
  lgPosledniA0B2 = b2;
  lgPosledniA0B3 = b3;
}

enum LgStartCekani : int8_t {
  LG_START_CEKA_DAL = 0,
  LG_START_POSLI_KROK2 = 1,
  LG_START_OPAK_KROK1 = 2,
  LG_START_SELHAL = 3
};

inline LgStartCekani lgVyhodnotStartCekani(unsigned long casOdKroku1) {
  unsigned long age = millis() - casOdKroku1;

  if (lgPosledniA0B3 == 0x08) {
    lgZapis.videnPrestartB3 = true;
  }

  if (lgZapis.videnPrestartB3 && lgPosledniA0B3 == 0x00 && age > 800) {
    return LG_START_OPAK_KROK1;
  }

  if (age < LG_START_CEKA_A0_MIN_MS) {
    return LG_START_CEKA_DAL;
  }

  if (lgPosledniA0B3 == 0x08 && (lgPosledniA0B2 & 0x02)) {
    return LG_START_POSLI_KROK2;
  }
  if (age >= LG_START_CEKA_A0_B3_MS && lgPosledniA0B3 == 0x08) {
    return LG_START_POSLI_KROK2;
  }

  if (age >= LG_START_CEKA_A0_MAX_MS) {
    return LG_START_OPAK_KROK1;
  }

  return LG_START_CEKA_DAL;
}

void lgZapisPridatKrok(uint8_t *out, uint8_t teplota, uint8_t sub, uint8_t b2, uint8_t b3);

void lgZapisNaplnStartKroky(uint8_t teplota) {
  lgZapis.pocetKroku = 0;
  lgZapisPridatKrok(nullptr, teplota, 0x32, 0x02, 0x00);
  lgZapisPridatKrok(nullptr, teplota, 0x32, 0x02, 0x02);
}

void lgZapisSpustDrzetPriprava(uint8_t teplota) {
  lgZapis.aktivni = false;
  lgZapis.jeStartSekvence = true;
  lgZapis.krok = 0;
  lgZapis.pocetKroku = 0;
  lgZapis.cekaA0Start = false;
  lgZapis.videnPrestartB3 = false;
  lgZapis.opakovani = 1;
  lgZapis.casDalsiho = millis() + 120;
  lgZapisPridatKrok(nullptr, teplota, 0x32, 0x00, 0x00);
  lgZapis.aktivni = (lgZapis.pocetKroku > 0);
}

inline bool lgJeStartPriprava(const uint8_t* p) {
  return p[1] == 0x32 && p[3] == 0x00;
}

inline bool lgJeStartPotvrzeni(const uint8_t* p) {
  return p[1] == 0x32 && p[2] == 0x02 && p[3] == 0x02;
}

inline bool lgSmimePoslatStartPotvrzeni() {
  return lgZapis.videnPrestartB3 && lgPosledniA0B3 == 0x08;
}

void lgZapisRestartStartOdKroku1(const char* duvod) {
  if (lgZapis.startPokus >= LG_START_MAX_POKUSU) {
    lgZapis.aktivni = false;
    lgZapis.cekaA0Start = false;
    Serial.printf("[ZAPIS] START selhal (%u pokusu): %s\n", lgZapis.startPokus, duvod);
    return;
  }
  lgZapis.startPokus++;
  lgZapis.krok = 0;
  lgZapis.cekaA0Start = false;
  lgZapis.videnPrestartB3 = false;
  lgZapis.jeStartSekvence = true;
  lgZapis.opakovani = 1;
  lgZapisNaplnStartKroky(lgZapis.startTeplota);
  lgZapis.aktivni = true;
  lgZapis.casDalsiho = millis() + LG_START_PO_ZRUSENI_MS;
  Serial.printf("[ZAPIS] START opakovani %u/%u — %s\n",
                lgZapis.startPokus, LG_START_MAX_POKUSU, duvod);
}

inline bool lgJeA0PripravenProStartKrok2(unsigned long casOdKroku1) {
  return lgVyhodnotStartCekani(casOdKroku1) == LG_START_POSLI_KROK2;
}

inline const char* lgPopisTcB3(uint8_t b3) {
  if (lgJeStabilniBeh(b3)) { return "ZAP+BEH"; }
  if (lgJeZapnuto(b3)) { return "ZAP"; }
  if (lgJePrestartB3(b3)) { return "PRESTART"; }
  return "VYP";
}

void prepniSoloRezim() {
  soloRezimTab5 = !soloRezimTab5;
  Serial.println(soloRezimTab5
    ? "\n[SOLO] ZAP — Tab5 smi posilat na sbernici (nezavisle na PARALLEL)"
    : "\n[SOLO] VYP — rucni TX vypnut (PARALLEL muze stale posilat obnovu)");
}

void lgNastavDrzenyStav(uint8_t teplota, bool zapnuto) {
  cilovaTeplotaTab5 = teplota;
  cilovyZapnutoTab5 = zapnuto;
  drzetStavAktivni = true;
  storageRequestSaveTcSession(zapnuto, teplota);
}

void lgZrusDrzenyStav() {
  drzetStavAktivni = false;
}

void prepniParallelRezim() {
  parallelRezimTab5 = !parallelRezimTab5;
  if (!parallelRezimTab5) {
    lgZrusDrzenyStav();
  }
  Serial.println(parallelRezimTab5
    ? "\n[PARALLEL] ZAP — Tab + dratovy ovladac spolecne na lince"
    : "\n[PARALLEL] VYP");
}

void prepniDrzetStav() {
  prepniParallelRezim();
}

void lgDokoncStartOdOrig(const char* zdroj) {
  if (!cekameNaOrigStart) { return; }
  cekameNaOrigStart = false;
  origPoslalStartPripravu = false;
  tcPozadavekZap = true;
  Serial.printf("[START] orig. dokoncil (%s) — Tab5 ustupuje\n", zdroj);
  lgZapis.aktivni = false;
  lgZapis.cekaA0Start = false;
  lgZapis.krok = 0;
  lgZapis.pocetKroku = 0;
  lgZrusDrzenyStav();
}

inline bool lgJeOrigStartPriprava(uint8_t b1, uint8_t b2, uint8_t b3) {
  return b1 == 0x32 && b2 == 0x02 && b3 == 0x00;
}

inline bool lgJeOrigStartPotvrzeni(uint8_t b1, uint8_t b2, uint8_t b3) {
  return b1 == 0x32 && b2 == 0x02 && b3 == 0x02;
}

inline bool lgJeOrigStartC0(uint8_t b1, uint8_t b2, uint8_t b3) {
  return lgJeOrigStartPriprava(b1, b2, b3) || lgJeOrigStartPotvrzeni(b1, b2, b3);
}

inline bool lgJeOrigStopAktivni(uint8_t b1, uint8_t b2, uint8_t b3) {
  if (b1 != 0x30) { return false; }
  return (b3 == 0x02) || ((b2 & 0x02) != 0);
}

void lgOrigPrevzalStart() {
  lgDokoncStartOdOrig("C0 32/02/02 od orig.");
}

void lgZrusCekaniOrig(const char* duvod) {
  if (!cekameNaOrigStart) { return; }
  cekameNaOrigStart = false;
  origPoslalStartPripravu = false;
  tcPozadavekZap = false;
  lgZapis.aktivni = false;
  lgZapis.cekaA0Start = false;
  lgZrusDrzenyStav();
  if (duvod != nullptr) {
    Serial.printf("[ORIG] cekani zruseno — %s\n", duvod);
  }
}

void lgUkonciCekaniProStop() {
  if (!cekameNaOrigStart) { return; }
  cekameNaOrigStart = false;
  origPoslalStartPripravu = false;
  lgZapis.aktivni = false;
  lgZapis.cekaA0Start = false;
}

void lgObsluhaCekaniOrig() {
  if (!cekameNaOrigStart) { return; }
  if (millis() - casCekaniOrigOdMs >= LG_ORIG_CEKANI_MAX_MS) {
    Serial.println("[ORIG] stale cekame — orig. jeste neposlal C0 32/02/xx (session bezi dal)");
    casCekaniOrigOdMs = millis();
  }
}

void lgAutoVypniDrzetPoCili(uint8_t b2, uint8_t b3) {
  if (!drzetStavAktivni || cilovyZapnutoTab5) { return; }
  if (!lgJeTcUplneVyp(b2, b3)) { return; }
  lgZrusDrzenyStav();
  Serial.println("[ZAPIS] drzeni STOP dokonceno — TC vypnuto");
}

void nastavMonitorPozastaven(bool pauza) {
  if (monitorPozastaven == pauza) { return; }
  monitorPozastaven = pauza;
  Serial.println(monitorPozastaven
    ? "\n[MONITOR] POZASTAVENO — log se neposila (P=pauza, R=spustit, mezernik=prepni)"
    : "\n[MONITOR] BEZI — logovani obnoveno");
}

void prepniMonitorPozastaven() {
  nastavMonitorPozastaven(!monitorPozastaven);
}

uint8_t vypocitejChecksum(uint8_t *data, uint8_t delkaBezChecksumu) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < delkaBezChecksumu; i++) { sum += data[i]; }
  return sum ^ 0x55;
}

const char* lgPopisC0Subcmd(uint8_t sub) {
  switch (sub) {
    case 0x30: return "STOP (orig. ovladac)";
    case 0x32: return "stav / teplota / START";
    case 0x33: return "zmena cilove teploty (+/-)";
    case 0x3C: return "nepouzivat";
    default:   return "neznama subcmd";
  }
}

void lgVypisStavB2B3(uint8_t b2, uint8_t b3, uint8_t sub = 0xFF) {
  Serial.printf("B2=0x%02X cerpadlo=%s | B3=0x%02X Tc=%s bin=",
                b2, (b2 & 0x02) ? "ANO" : "NE ",
                b3, lgPopisTcB3(b3));
  for (int bit = 7; bit >= 0; bit--) { Serial.print((b3 >> bit) & 1); }
  Serial.println();
  if (sub == 0xFF && lgJePrestartB3(b3)) {
    if (b2 & 0x02) {
      Serial.println("  -> B3=0x08 + cerpadlo (cyklus po SP / nebo START-cekani)");
    } else {
      Serial.println("  -> B3=0x08 (cyklus pauza / priprava monobloku)");
    }
  } else if (sub == 0xFF && lgJeCerpadloZap(b2) && b3 == 0x00) {
    Serial.println("  -> jen PROTOCENI obehoveho cerpadla (B3=0)");
  } else if (sub == 0x30 && b2 == 0x00 && b3 == 0x02) {
    Serial.println("  -> STOP krok 1 (prechod B3=0x02 — jeste neni vypnuto)");
  } else if (sub == 0x30 && b2 == 0x00 && b3 == 0x00) {
    Serial.println("  -> STOP krok 2 / Tc vypnuto");
  } else if (sub == 0x32 && b2 == 0x00 && b3 == 0x00) {
    Serial.println("  -> START krok 1 (priprava B3=0x00, bez cerpadla)");
  } else if (sub == 0x32 && b2 == 0x02 && b3 == 0x00) {
    Serial.println("  -> START krok 1 (priprava B2=0x02 B3=0x00 — spousti PRESTART)");
  } else if (sub == 0x32 && b2 == 0x02 && b3 == 0x02) {
    Serial.println("  -> START krok 2 / potvrzeni prikazu (B3=0x02)");
  } else if (sub == 0x33) {
    Serial.printf("  -> zmena cilove teploty (A0 B1=0x33, B2/B3=%02X/%02X)\n", b2, b3);
  } else if (lgJeStabilniBeh(b3)) {
    Serial.println("  -> stabilni beh (B3 ma bity 0x02+0x08 = 0x0A v A0)");
  } else if (lgJeZapnuto(b3) && !(b3 & LG_BIT_BEZI)) {
    Serial.println("  -> prikaz ZAP / rozjezd (bit 0x02, jeste bez 0x08)");
  }
}

void lgVypisDelta(const char* popis, uint8_t *stary, uint8_t *novy, uint8_t delka) {
  bool nejakaZmena = false;
  for (uint8_t i = 0; i < delka; i++) {
    if (stary[i] == novy[i]) { continue; }
    if (!nejakaZmena) {
      Serial.printf(">>> ZMENA oproti predchozimu %s:\n", popis);
      nejakaZmena = true;
    }
    Serial.printf("    B%-2u : %02X -> %02X", i, stary[i], novy[i]);
    if (i == 1) { Serial.print("  <-- C0 subcmd"); }
    if (i == 2) { Serial.print("  <-- priprava / cerpadlo"); }
    if (i == 3) { Serial.print("  <-- START/STOP (bit 0x02)"); }
    if (i == 8) { Serial.print("  <-- cilova teplota"); }
    if (i == 9) { Serial.print("  <-- max. topna voda (typ/konfig)"); }
    if (i == 13 || i == 14) { Serial.print("  <-- typ Tc (konfig zarizeni)"); }
    if (i == 19) { Serial.print("  <-- checksum (B19)"); }
    Serial.println();
  }
  if (!nejakaZmena) {
    Serial.printf(">>> %s: zadna zmena oproti predchozimu paketu\n", popis);
  }
}

void lgDekodujA0(uint8_t *data, uint8_t len) {
  Serial.println("TYP: A0 monoblok (stav venkovni jednotky)");
  if (len < 15) {
    Serial.printf("  (kratky paket, jen %u B)\n", len);
    return;
  }
  Serial.print("  ");
  lgVypisStavB2B3(data[2], data[3]);
  Serial.printf("  B4=0x%02X | B8 cilova=%u C | B9 max_voda=%u C | B11 vstup=%u | B12 vystup=%u | B13-B14 typ=0x%02X%02X | B19 checksum=0x%02X\n",
                data[4], data[8], data[9], data[11], data[12], data[13], data[14],
                len >= 20 ? data[19] : 0);
}

void lgDekodujC0(uint8_t *data, uint8_t len) {
  uint8_t sub = (len >= 2) ? data[1] : 0xFF;
  Serial.printf("TYP: C0 ovladac  subcmd=0x%02X (%s)\n", sub, lgPopisC0Subcmd(sub));
  if (len >= 4) {
    Serial.print("  ");
    lgVypisStavB2B3(data[2], data[3], sub);
  }
  if (len >= 9) {
    Serial.printf("  B4=0x%02X | B8 teplota=%u C | B11=%u B12=%u\n",
                  data[4], data[8], len >= 12 ? data[11] : 0, len >= 13 ? data[12] : 0);
  }
}

bool lgJeToNasC0(uint8_t *data, uint8_t len) {
  if (!mamePosledniNasTx || len != LG_PAKET_LEN) { return false; }
  if (millis() - casPosledniNasTx > 8000) { return false; }
  return memcmp(data, posledniNasTx, LG_PAKET_LEN) == 0;
}

void lgSledujFaziStartuA0(uint8_t b2, uint8_t b3) {
  if (b2 == a0PosledniB2 && b3 == a0PosledniB3) { return; }

  const bool nasStart = lgZapis.aktivni && lgZapis.jeStartSekvence;
  const char* faze;
  if (lgJeStabilniBeh(b3)) {
    faze = "STABILNI BEH (kompresor, B3=0x0A)";
  } else if (lgJePrestartB3(b3)) {
    if (nasStart) {
      faze = (b2 & 0x02)
          ? "START-CEKANI (B3=0x08, cerpadlo — ceka C0 32/02/02)"
          : "START-CEKANI (B3=0x08)";
    } else {
      faze = (b2 & 0x02)
          ? "CYKLUS pauza (B3=0x08 + cerpadlo — po SP vody)"
          : "CYKLUS pauza (B3=0x08 — po SP vody)";
    }
  } else if (lgJeZapnuto(b3) && (b2 & 0x02)) {
    faze = "ROZJEZD (cerpadlo+prikaz, B3=0x02)";
  } else if (lgJeZapnuto(b3)) {
    faze = "ROZJEZD (prikaz ZAP, cerpadlo OFF)";
  } else if (lgJeCerpadloZap(b2)) {
    faze = "PROTOCENI (jen obehove cerpadlo, B3=0)";
  } else {
    faze = "VYP";
  }

  Serial.printf("[FAZE A0] B2=0x%02X B3=0x%02X => %s", b2, b3, faze);
  if (a0PosledniB2 != 0xFF) {
    Serial.printf("  (predchozi B2=0x%02X B3=0x%02X)", a0PosledniB2, a0PosledniB3);
  }
  Serial.println();

  if (lgJeStabilniBeh(b3) && mamePosledniNasTx && (millis() - casPosledniStartTx) < 12000) {
    if (a0PosledniB3 == 0x00 && a0PosledniB2 == 0x00) {
      Serial.println("[FAZE A0] VAROVANI: preskok na STABILNI bez rozjezdu — mozny okamzity start kompresoru!");
    } else if (a0PosledniB3 == 0x02 && a0PosledniB2 == 0x02 &&
               (millis() - casPosledniStartTx) < 3000) {
      Serial.println("[FAZE A0] VAROVANI: STABILNI prilis brzy po START (< 3 s od potvrzeni)!");
    }
  }

  if (lgJeZapnuto(a0PosledniB3) && lgJeStabilniBeh(b3) &&
      (a0PosledniB2 & 0x02) && !(b2 & 0x02)) {
    Serial.println("[FAZE A0] cerpadlo se vypnulo pred stabilnim behom (typicky orig. sekvence)");
  }

  a0PosledniB2 = b2;
  a0PosledniB3 = b3;
}

void lgUlozSnimky(uint8_t *data, uint8_t delka) {
  switch (data[0]) {
    case 0xA0:
      if (delka <= sizeof(predchoziA0)) {
        memcpy(predchoziA0, data, delka);
        predchoziA0Len = delka;
      }
      break;
    case 0xC0:
      if (delka <= sizeof(predchoziC0)) {
        memcpy(predchoziC0, data, delka);
        predchoziC0Len = delka;
      }
      break;
    default:
      break;
  }
}

void lgSestavC0ZVzoru(uint8_t *out, uint8_t teplota, uint8_t subcmd, uint8_t b2, uint8_t b3) {
  if (predchoziC0Len >= LG_PAKET_LEN) {
    memcpy(out, predchoziC0, LG_PAKET_LEN);
  } else {
    uint8_t vychozi[LG_PAKET_LEN] = {
      0xC0, 0x32, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
      0x28, 0x32, 0x00, 0x14, 0x15, 0xC3, 0xC3, 0x00,
      0x00, 0x00, 0x00, 0x00
    };
    memcpy(out, vychozi, LG_PAKET_LEN);
  }

  out[0] = 0xC0;
  out[1] = subcmd;
  out[2] = b2;
  out[3] = b3;
  out[4] = 0x40;
  out[8] = teplota;
  out[9] = 0x32;
  out[10] = 0x00;
  out[11] = mVstupni;
  out[12] = mVystupni;
  out[15] = 0x00;
  out[16] = 0x00;
  out[17] = 0x00;
  out[18] = 0x00;
  out[19] = vypocitejChecksum(out, 19);
}

void lgOdesliC0Paket(uint8_t *paket) {
  ESP_LOGI("LIN", "TX C0 sub=0x%02X B2=%02X B3=%02X B8=%u",
           paket[1], paket[2], paket[3], (unsigned)paket[8]);

#if LG_ODPOSLECH_BEZ_ZAPISU
  if (!lgSmiPosilatTx()) {
    ESP_LOGI("LIN", "TX simulace — zapnete PARALLEL nebo SOLO");
    return;
  }
  LG_Serial.write(paket, LG_PAKET_LEN);
  LG_Serial.flush();
#else
  LG_Serial.write(paket, LG_PAKET_LEN);
  LG_Serial.flush();
#endif

  memcpy(posledniNasTx, paket, LG_PAKET_LEN);
  casPosledniNasTx = millis();
  mamePosledniNasTx = true;

  if (paket[1] == 0x32 && paket[2] == 0x02 && paket[3] == 0x02) {
    casPosledniStartTx = millis();
  }
}

void lgZapisPridatKrok(uint8_t *out, uint8_t teplota, uint8_t sub, uint8_t b2, uint8_t b3) {
  if (lgZapis.pocetKroku >= 4) { return; }
  lgSestavC0ZVzoru(lgZapis.pakety[lgZapis.pocetKroku], teplota, sub, b2, b3);
  lgZapis.pocetKroku++;
}

void lgZapisSpust(uint8_t teplota, bool zapnuto, bool zmenaStartu) {
#if LG_BLOKOVAT_ZAPIS_S_ORIG
  if (!soloRezimTab5 && !drzetStavAktivni && origOvladacDetekovan && zmenaStartu) {
    Serial.println("\n[ZAPIS BLOKOVAN] Bez aktivniho drzeni — stisknete START znovu.");
    return;
  }
#endif

  if (zmenaStartu && zapnuto && !lgZapisPovolenStart && !cekameNaOrigStart) {
    Serial.println("[ZAPIS BLOKOVAN] START jen po tlacitku START (HMI/MQTT/plan)");
    return;
  }
  lgZapisPovolenStart = false;

  if (zmenaStartu && zapnuto && !soloRezimTab5) {
    cekameNaOrigStart = true;
    origPoslalStartPripravu = false;
    tcPozadavekZap = true;
    casCekaniOrigOdMs = millis();
    lgNastavDrzenyStav(teplota, true);
    Serial.println("[ZAPIS] START — 1x C0 32/00/00, pak stisknete START na orig.");
    lgZapisSpustDrzetPriprava(teplota);
    return;
  }

  if (zmenaStartu && parallelRezimTab5) {
    lgNastavDrzenyStav(teplota, zapnuto);
  }

  lgZapis.aktivni = false;
  lgZapis.krok = 0;
  lgZapis.pocetKroku = 0;
  lgZapis.cekaA0Start = false;
  lgZapis.opakovani = 2;

  if (zmenaStartu && zapnuto) {
    lgZapis.jeStartSekvence = true;
    lgZapis.opakovani = 1;
    lgZapis.startPokus = 1;
    lgZapis.startTeplota = teplota;
    lgZapis.videnPrestartB3 = false;
    Serial.println("[ZAPIS] START sekvence (2 kroky jako orig.):");
    Serial.println("  1) C0 32 B2=02 B3=00  (priprava -> A0 B3=0x08 PRESTART)");
    Serial.println("  2) cekat min 5 s + B3=0x08 v A0");
    Serial.println("  3) C0 32 B2=02 B3=02  (potvrzeni — JEN pokud B3=0x08!)");
    lgZapisNaplnStartKroky(teplota);
  } else if (zmenaStartu && !zapnuto) {
    lgZapis.jeStartSekvence = true;
    lgZapis.opakovani = 2;
    Serial.println("[ZAPIS] STOP sekvence (jako orig.): 30/00/02 -> 30/00/00 x2");
    lgZapisPridatKrok(nullptr, teplota, 0x30, 0x00, 0x02);
    lgZapisPridatKrok(nullptr, teplota, 0x30, 0x00, 0x00);
  } else {
    lgZapis.jeStartSekvence = false;
    // Orig. +/- posila C0 33 + B8; A0 pak ma B1=0x33. NE C0 32/02/02 (= START potvrzeni).
    uint8_t b2 = lgPosledniA0B2;
    uint8_t b3 = lgPosledniA0B3;
    if (!zapnuto && !lgJeCerpadloZap(b2)) {
      b2 = 0x00;
      b3 = 0x00;
    } else if (lgJeCerpadloZap(b2) && !lgJeZapnuto(b3) && !lgJePrestartB3(b3)) {
      b2 = 0x02;
      b3 = 0x00;
    }
    Serial.printf("[ZAPIS] Teplota=%u C (C0 33 B2=%02X B3=%02X) x1\n", teplota, b2, b3);
    lgZapisPridatKrok(nullptr, teplota, 0x33, b2, b3);
    lgZapis.opakovani = 1;
    lgZapis.casDalsiho = millis();
  }

  if (zmenaStartu) {
    lgZapis.casDalsiho = millis() + 120;
  }

  lgZapis.aktivni = (lgZapis.pocetKroku > 0);
}

static unsigned long lgPosledniStartRestartMs = 0;

void lgReagujNaOrigStopBehemStartu(uint8_t *data, uint8_t delka) {
  if (!lgZapis.aktivni || !lgZapis.jeStartSekvence || !lgZapis.cekaA0Start ||
      delka < 2) {
    return;
  }
  if (data[0] != 0xC0 || lgJeToNasC0(data, delka)) { return; }
  if (data[1] != 0x30) { return; }
  if (millis() - lgPosledniStartRestartMs < 3000) { return; }
  casPosledniC0Orig = millis();
  lgZapisRestartStartOdKroku1("orig. STOP (C0 30) behem PRESTART");
  lgPosledniStartRestartMs = millis();
}

void lgKontrolujPrestartA0(uint8_t *data, uint8_t delka) {
  if (!lgZapis.aktivni || !lgZapis.jeStartSekvence || !lgZapis.cekaA0Start ||
      delka < 4 || data[0] != 0xA0) {
    return;
  }
  LgStartCekani vysledek = lgVyhodnotStartCekani(lgZapis.casCekaniStartOd);
  if (vysledek == LG_START_OPAK_KROK1) {
    if (millis() - lgPosledniStartRestartMs < 3000) { return; }
    lgZapisRestartStartOdKroku1("A0 PRESTART zrusen (B3 0x08->0x00)");
    lgPosledniStartRestartMs = millis();
  }
}

void lgReagujNaOrigC0(uint8_t *data, uint8_t delka) {
  if (delka < 4 || lgJeToNasC0(data, delka)) { return; }

  if (cekameNaOrigStart) {
    casCekaniOrigOdMs = millis();
    if (lgJeOrigStartPotvrzeni(data[1], data[2], data[3])) {
      lgOrigPrevzalStart();
    } else if (lgJeOrigStartPriprava(data[1], data[2], data[3])) {
      origPoslalStartPripravu = true;
      Serial.println("[ORIG] C0 32/02/00 — orig. spousti, cekame potvrzeni / PRESTART");
    } else if (lgJeOrigStopAktivni(data[1], data[2], data[3])) {
      Serial.println("[ORIG] aktivni STOP od orig. behem cekani — cekame dal (idle C0 30/00/00 ignorujeme)");
    }
    return;
  }

  if (!parallelRezimTab5 || !lgSmiPosilatTx()) { return; }

  bool origVyp = (data[1] == 0x30) || !lgJeZapnuto(data[3]);
  bool origZap = lgJeZapnuto(data[3]) && (data[1] == 0x32);
  casPosledniC0Orig = millis();

  if (lgZapisAktivni() || pozadavekNaZapis) { return; }

  bool konflikt = (cilovyZapnutoTab5 && origVyp) || (!cilovyZapnutoTab5 && origZap);
  if (!konflikt) { return; }

  if (millis() - casPosledniAutoObnova < LG_AUTO_OBNOVA_MIN_MS) { return; }

  Serial.printf("\n[PARALLEL] Orig. poslal C0 0x%02X (%s) — odpovidame %s sekvenci\n",
                data[1], origVyp ? "STOP/VYP" : "stav",
                cilovyZapnutoTab5 ? "START" : "STOP");
  lgZapisSpust(cilovaTeplotaTab5, cilovyZapnutoTab5, true);
  casPosledniAutoObnova = millis();
}

/**
 * Dříve: auto-adopt session z A0 (topení / oběhové čerpadlo).
 * Session smí být ON jen po START (HMI, MQTT, plán) — ne z rána protocení vody.
 */
inline void lgAdoptujSessionZA0(uint8_t *data, uint8_t delka) {
  (void)data;
  (void)delka;
}

void lgZkontrolujObnovuStavu(uint8_t *data, uint8_t delka) {
  if (delka < 4) { return; }

  if (data[0] == 0xC0 && !lgJeToNasC0(data, delka)) {
    lgReagujNaOrigStopBehemStartu(data, delka);
    if (!parallelRezimTab5 || !lgSmiPosilatTx()) { return; }
    lgReagujNaOrigC0(data, delka);
    return;
  }

  if (data[0] == 0xA0) {
    lgAdoptujSessionZA0(data, delka);
    lgKontrolujPrestartA0(data, delka);
    lgAutoVypniDrzetPoCili(data[2], data[3]);

    if (cekameNaOrigStart && origPoslalStartPripravu) {
      if (lgJeStabilniBeh(data[3])) {
        lgDokoncStartOdOrig("A0 stabilni beh B3=0x0A");
      } else if (lgJePrestartB3(data[3]) && lgJeCerpadloZap(data[2])) {
        lgDokoncStartOdOrig("orig. PRESTART cerpadlo");
      }
    }
    if (lgJeTcProvoz(data[2], data[3]) && !cekameNaOrigStart && cilovyZapnutoTab5) {
      // Jen topný cyklus / B3=0x08 — ne samotné protocení čerpadla; jen po START
      tcPozadavekZap = true;
    } else if (!lgJeTcProvoz(data[2], data[3]) && !cekameNaOrigStart &&
               !lgZapisAktivni() && !cilovyZapnutoTab5) {
      // Při session ON necháme tcPozadavekZap (cyklus s pumpou mezi kompresory)
      tcPozadavekZap = false;
    }
  }

  if (cekameNaOrigStart) { return; }
  if (!drzetStavAktivni || !lgSmiPosilatTx()) { return; }
  if (lgZapisAktivni() || pozadavekNaZapis) { return; }
  if (data[0] != 0xA0) { return; }

  if (casPosledniC0Orig && (millis() - casPosledniC0Orig) < LG_AUTO_OBNOVA_PO_ORIG_MS) {
    return;
  }
  if (millis() - casPosledniAutoObnova < LG_AUTO_OBNOVA_MIN_MS) { return; }

  const bool topiNeboCyklus = lgJeTcProvoz(data[2], data[3]);
  const bool cilStav = cilovyZapnutoTab5;

  // Session OFF: nikdy neposílat START/STOP podle A0 (SOLO = HMI je master).
  if (!cilStav) {
    if (soloRezimTab5 && topiNeboCyklus) {
      Serial.printf(
          "\n[SESSION] SOLO: A0=topi ale session=VYP — NEPosilam auto-START (cekam HMI)\n");
    }
    return;
  }

  // Session ON: neauto-startovat z klidu — obnova jen po explicitním START z HMI.
  (void)topiNeboCyklus;
  return;
}

void lgZapisObsluha() {
  if (!lgZapis.aktivni) { return; }
  if (millis() < lgZapis.casDalsiho) { return; }
  const unsigned long quietMs = lgZapis.jeStartSekvence ? 60UL : 0UL;
  if (indexLg > 0 || (quietMs > 0 && (millis() - posledniBajtMs) < quietMs)) {
    lgZapis.casDalsiho = millis() + 5;
    return;
  }

  if (lgZapis.cekaA0Start && lgZapis.jeStartSekvence) {
    LgStartCekani vysledek = lgVyhodnotStartCekani(lgZapis.casCekaniStartOd);
    if (vysledek == LG_START_CEKA_DAL) {
      lgZapis.casDalsiho = millis() + 400;
      return;
    }
    if (vysledek == LG_START_OPAK_KROK1) {
      if (millis() - lgPosledniStartRestartMs < 3000) {
        lgZapis.casDalsiho = millis() + 400;
        return;
      }
      lgZapisRestartStartOdKroku1("PRESTART timeout / zrusen");
      lgPosledniStartRestartMs = millis();
      return;
    }
    if (lgPosledniA0B3 != 0x08) {
      lgZapis.casDalsiho = millis() + 400;
      return;
    }
    lgZapis.cekaA0Start = false;
    lgZapis.krok++;
    Serial.printf("[ZAPIS] A0 PRESTART ok (B2=0x%02X B3=0x08) — posilam potvrzeni C0 32/02/02\n",
                  lgPosledniA0B2);
  }

  uint8_t sentKrok = lgZapis.krok;
  uint8_t *pkt = lgZapis.pakety[sentKrok];

  if (lgZapis.jeStartSekvence && lgJeStartPotvrzeni(pkt) &&
      !lgSmimePoslatStartPotvrzeni()) {
    Serial.printf("[ZAPIS] BLOKOVANO potvrzeni — A0 B3=0x%02X (potreba 0x08 PRESTART)\n",
                  lgPosledniA0B3);
    lgZapis.krok = 0;
    lgZapis.cekaA0Start = true;
    lgZapis.casCekaniStartOd = millis();
    lgZapis.casDalsiho = millis() + 400;
    return;
  }

  lgOdesliC0Paket(pkt);
  uint8_t *sent = pkt;

  if (lgZapis.jeStartSekvence && lgJeStartPotvrzeni(sent)) {
    Serial.println("[ZAPIS] Potvrzeni START C0 32/02/02 odeslano");
  }

  bool startKrok1 = lgZapis.jeStartSekvence && lgJeStartPriprava(sent);
  bool maStartKrok2 =
      lgZapis.jeStartSekvence &&
      (lgZapis.pocetKroku > sentKrok + 1 &&
       lgJeStartPotvrzeni(lgZapis.pakety[sentKrok + 1]));

  if (startKrok1 && maStartKrok2) {
    lgZapis.cekaA0Start = true;
    lgZapis.casCekaniStartOd = millis();
    lgZapis.casDalsiho = millis() + 400;
    Serial.println("[ZAPIS] Krok 1 odeslan — cekam A0 B3=0x08 (orig. PRESTART)...");
    return;
  }

  lgZapis.krok++;

  if (lgZapis.krok >= lgZapis.pocetKroku) {
    lgZapis.krok = 0;
    lgZapis.opakovani--;
    if (lgZapis.opakovani == 0) {
      lgZapis.aktivni = false;
      Serial.println("[ZAPIS] Sekvence dokoncena.");
      return;
    }
  }

  unsigned long delayMs = LG_ZAPIS_MEZI_KROKY_MS;
  if (lgZapis.jeStartSekvence && sent[1] == 0x32 && sent[2] == 0x02 &&
      sent[3] == 0x02 && lgZapis.krok >= lgZapis.pocetKroku &&
      lgZapis.opakovani > 0) {
    delayMs = LG_START_OPAK_MS;
  }

  lgZapis.casDalsiho = millis() + delayMs;
}

void odposlechSbernice(uint8_t *data, uint8_t delka) {
  pocetPaketu++;

#if LG_KRATKY_LOG
  static uint32_t s_lastFullA0Ms = 0;
  if (data[0] == 0xA0 && predchoziA0Len == delka && delka > 0 &&
      memcmp(data, predchoziA0, delka) == 0) {
    const bool dueFull =
        (s_lastFullA0Ms == 0) ||
        ((millis() - s_lastFullA0Ms) >= LG_KRATKY_FULL_EVERY_MS);
    Serial.printf("[A0 #%lu t=%lu] bez zmeny B2=%02X B3=%02X (%s)%s\n",
                  pocetPaketu, millis(), delka >= 3 ? data[2] : 0,
                  delka >= 4 ? data[3] : 0, delka >= 4 ? lgPopisTcB3(data[3]) : "?",
                  dueFull ? " → plny dump" : "");
    if (delka >= 4) {
      lgAktualizujPosledniA0(data[2], data[3]);
      lgSledujFaziStartuA0(data[2], data[3]);
    }
    if (!dueFull) {
      lgUlozSnimky(data, delka);
      lgZkontrolujObnovuStavu(data, delka);
      return;
    }
    // dueFull: pokračuj na plný výpis (uloží snapshot až tam)
  }
  if (data[0] == 0xA0) {
    s_lastFullA0Ms = millis();
  }
#endif

  if (monitorPozastaven) {
    lgUlozSnimky(data, delka);
    return;
  }

  Serial.printf("\n========== SBĚRNICE #%lu | t=%lu ms | len=%u B ==========\n",
                pocetPaketu, millis(), delka);

  Serial.print("HEX: ");
  for (uint8_t i = 0; i < delka; i++) { Serial.printf("%02X ", data[i]); }
  Serial.println();

  if (delka >= 2) {
    uint8_t ocekavany = vypocitejChecksum(data, delka - 1);
    uint8_t skutecny = data[delka - 1];
    Serial.printf("CHECKSUM: %s (ocekavano %02X, prijato %02X)  [soucet XOR 0x55]\n",
                  ocekavany == skutecny ? "OK" : "CHYBA!", ocekavany, skutecny);
  }

  switch (data[0]) {
    case 0xA0:
      if (delka >= 4) {
        lgAktualizujPosledniA0(data[2], data[3]);
      }
      lgDekodujA0(data, delka);
      if (delka >= 4) {
        lgSledujFaziStartuA0(data[2], data[3]);
      }
      if (predchoziA0Len > 0) {
        uint8_t cmpLen = delka < predchoziA0Len ? delka : predchoziA0Len;
        lgVypisDelta("A0", predchoziA0, data, cmpLen);
        if (delka >= 4 && predchoziA0Len >= 4 &&
            lgJeZapnuto(predchoziA0[3]) != lgJeZapnuto(data[3])) {
          Serial.println("*** START/STOP se zmenil v A0 ***");
        }
        if (delka >= 4 && predchoziA0Len >= 4 &&
            !lgJeStabilniBeh(predchoziA0[3]) && lgJeStabilniBeh(data[3])) {
          Serial.println("*** A0: Tc prechazi do STABILNIHO BEHU (B3=0x0A) ***");
        }
      }
      lgUlozSnimky(data, delka);
      break;

    case 0xC0: {
      bool odNas = lgJeToNasC0(data, delka);
      if (!odNas) {
        origOvladacDetekovan = true;
        if (mamePosledniNasTx && (millis() - casPosledniNasTx) < 60000) {
          bool myZap = lgJeZapnuto(posledniNasTx[3]);
          bool rxZap = lgJeZapnuto(data[3]);
          if (myZap && !rxZap) {
            Serial.println("!!! KONFLIKT: orig. ovladac PREPSAL Tab5 START -> STOP !!!");
          } else if (!myZap && rxZap) {
            Serial.println("!!! KONFLIKT: orig. ovladac PREPSAL Tab5 STOP -> START !!!");
          }
        }
        Serial.println("  (C0 z orig. wall controlleru)");
      } else {
        Serial.println("  (C0 echo naseho TX)");
      }

      lgDekodujC0(data, delka);
      if (predchoziC0Len > 0) {
        uint8_t cmpLen = delka < predchoziC0Len ? delka : predchoziC0Len;
        lgVypisDelta("C0", predchoziC0, data, cmpLen);
        if (!odNas && delka >= 4 && predchoziC0Len >= 4 &&
            lgJeZapnuto(predchoziC0[3]) != lgJeZapnuto(data[3])) {
          Serial.println("*** START/STOP zmenen orig. ovladacem ***");
        }
      }
      lgUlozSnimky(data, delka);
      break;
    }

    default:
      Serial.printf("TYP: neznamy header 0x%02X\n", data[0]);
      break;
  }
  Serial.println("==================================================");
  lgZkontrolujObnovuStavu(data, delka);
}

void odposlechSberniceTichy(uint8_t *data, uint8_t delka) {
  pocetPaketu++;
  if (delka >= 4 && data[0] == 0xA0) {
    lgAktualizujPosledniA0(data[2], data[3]);
    lgSledujFaziStartuA0(data[2], data[3]);
  }
  lgUlozSnimky(data, delka);
  lgZkontrolujObnovuStavu(data, delka);
}

void provedZapisTeploty(uint8_t teplota, bool zapnuto, bool zmenaStartu) {
  lgZapisSpust(teplota, zapnuto, zmenaStartu);
}

#endif
