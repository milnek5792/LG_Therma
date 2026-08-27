// lg_protocol.h — dekodovani A0/C0 (bez stavu fronty zapisu)
#ifndef LG_PROTOCOL_H
#define LG_PROTOCOL_H

#include <Arduino.h>
#include "bus_lg_model.h"

#define LG_BIT_ZAPNUTO 0x02
#define LG_BIT_BEZI    0x08

inline bool lgJeZapnuto(uint8_t b3) { return (b3 & LG_BIT_ZAPNUTO) != 0; }
inline bool lgJeStabilniBeh(uint8_t b3) { return lgJeZapnuto(b3) && (b3 & LG_BIT_BEZI) != 0; }
inline bool lgJeCerpadloZap(uint8_t b2) { return (b2 & 0x02) != 0; }

/**
 * B3==0x08 — LG „pauza / příprava“:
 *  - při našem START: čekání na C0 32/02/02
 *  - za běhu session: běžný cyklus po dosažení SP vody (pumpa on/off, pak znovu kompresor)
 * Samotné B3=0x08 NENÍ důvod brát to jako „čerstvý PRESTART start sekvence“ v UI.
 */
inline bool lgJePrestartB3(uint8_t b3) { return b3 == 0x08; }

/** Kompresor běží / rozjezd — ne jen B3=0x0A (často zůstává B3=0x02). */
inline bool lgJeKompresorBezi(uint8_t b3) {
  if (lgJeStabilniBeh(b3)) {
    return true;
  }
  // ROZJEZD / provoz: bit ZAPNUTO, ne čistý B3=0x08 cyklus
  return lgJeZapnuto(b3) && !lgJePrestartB3(b3);
}

/**
 * Topný provoz / cyklus TČ (ne samotné protocení okruhu).
 * Protocení = typicky B2=0x02, B3=0x00 → false.
 * B3=0x08 (pauza po SP) / ZAP / stabilní běh → true.
 */
inline bool lgJeTcProvoz(uint8_t b2, uint8_t b3) {
  (void)b2;
  return lgJePrestartB3(b3) || lgJeZapnuto(b3) || lgJeStabilniBeh(b3);
}

/** Úplně v klidu — ani topný cyklus, ani oběhové čerpadlo. */
inline bool lgJeTcUplneVyp(uint8_t b2, uint8_t b3) {
  return !lgJeTcProvoz(b2, b3) && !lgJeCerpadloZap(b2);
}

/** Sběrnice „živá“ pro session ON (včetně krátkého protocení mezi cykly). */
inline bool lgJeTcSessionZiva(uint8_t b2, uint8_t b3) {
  return lgJeTcProvoz(b2, b3) || lgJeCerpadloZap(b2);
}

inline bool lgJeTcZapnutoProUi(uint8_t b2, uint8_t b3) {
  if (lgJeTcProvoz(b2, b3)) {
    return true;
  }
  if (cekameNaOrigStart) {
    return false;
  }
  return tcPozadavekZap;
}

inline bool lgTcBeziNaSbernici(uint8_t b2, uint8_t b3) {
  if (soloRezimTab5) {
    return lgJeTcProvoz(b2, b3);
  }
  return lgJeTcZapnutoProUi(b2, b3);
}

#endif
