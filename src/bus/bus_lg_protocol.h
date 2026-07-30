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
inline bool lgJePrestartB3(uint8_t b3) { return b3 == 0x08; }

inline bool lgJeTcProvoz(uint8_t b2, uint8_t b3) {
  return lgJeCerpadloZap(b2) || lgJePrestartB3(b3) || lgJeZapnuto(b3) || lgJeStabilniBeh(b3);
}

inline bool lgJeTcUplneVyp(uint8_t b2, uint8_t b3) {
  return !lgJeTcProvoz(b2, b3);
}

inline bool lgJeTcZapnutoProUi(uint8_t b2, uint8_t b3) {
  if (lgJeTcProvoz(b2, b3)) { return true; }
  if (cekameNaOrigStart) { return false; }
  return tcPozadavekZap;
}

inline bool lgTcBeziNaSbernici(uint8_t b2, uint8_t b3) {
  if (soloRezimTab5) { return lgJeTcProvoz(b2, b3); }
  return lgJeTcZapnutoProUi(b2, b3);
}

#endif
