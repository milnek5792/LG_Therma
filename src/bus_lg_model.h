// bus_lg_model.h — sdileny stav mezi LIN a UI (mutex)
#ifndef LG_MODEL_H
#define LG_MODEL_H

#include <Arduino.h>
#include <HardwareSerial.h>

extern HardwareSerial LG_Serial;
extern uint8_t bufferLg[64];
extern uint8_t indexLg;
extern unsigned long posledniBajtMs;

extern uint8_t mCilova, mVstupni, mVystupni;
extern bool stavZapnuto;
extern volatile bool pozadavekNaZapis;
extern bool pozadavekZmenaStartu;
extern uint8_t novaCilovaTeplota;
extern bool bliknuti;
extern bool monitorPozastaven;
extern bool origOvladacDetekovan;
extern bool soloRezimTab5;
/** PARALLEL: Tab + drátový ovladač na jedné lince. */
extern bool parallelRezimTab5;
extern bool drzetStavAktivni;
extern bool cilovyZapnutoTab5;
extern bool cekameNaOrigStart;
extern bool tcPozadavekZap;
extern uint8_t cilovaTeplotaTab5;
/** Jednorázové povolení LIN START TX (HMI/MQTT/plán). */
extern bool lgZapisPovolenStart;

extern char posledniStavovyText[64];
extern volatile bool potrebaObnovitDisplej;

void lgModelInit();
void lgModelLock();
void lgModelUnlock();

void nastavStavovyText(const char* text);

void lgModelSnapA0(const uint8_t* data, uint8_t len);
void lgModelSnapA0Locked(const uint8_t* data, uint8_t len);
uint8_t lgModelA0Bajt(uint8_t idx, uint8_t vychozi = 0);
bool lgMaCerstoA0(uint32_t maxAgeMs = 0);

typedef struct {
  bool lin_live;
  uint8_t b2;
  uint8_t b3;
  uint8_t a0_sp;
  uint8_t m_vstupni;
  uint8_t m_vystupni;
  uint8_t m_cilova;
  uint8_t nova_cilova;
  bool pozadavek_zapis;
  bool cilovy_zapnuto;
  bool cekame_orig;
  bool tc_pozadavek;
} LgModelUiSnap;

void lgModelReadUiSnap(LgModelUiSnap* out);
void lgModelRestoreSessionFromNvs(void);

#endif
