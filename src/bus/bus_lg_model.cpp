#include "bus_lg_model.h"
#include "bus_lg_config.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

HardwareSerial LG_Serial(LG_UART_NUM);
uint8_t bufferLg[64];
uint8_t indexLg = 0;
unsigned long posledniBajtMs = 0;

uint8_t mCilova = 0, mVstupni = 0, mVystupni = 0;
bool stavZapnuto = false;
volatile bool pozadavekNaZapis = false;
bool pozadavekZmenaStartu = false;
uint8_t novaCilovaTeplota = 0;
bool bliknuti = false;
bool monitorPozastaven = false;
bool origOvladacDetekovan = false;
bool soloRezimTab5 = false;
bool parallelRezimTab5 = false;
bool drzetStavAktivni = false;
bool cilovyZapnutoTab5 = false;
bool cekameNaOrigStart = false;
bool tcPozadavekZap = false;
uint8_t cilovaTeplotaTab5 = 0;

char posledniStavovyText[64] = "Odposlech sbernice - cekam...";
volatile bool potrebaObnovitDisplej = false;

static SemaphoreHandle_t lgModelMu;
static uint8_t a0Snap[20];
static uint8_t a0SnapLen = 0;
static unsigned long s_casPosledniA0Ms = 0;

void lgModelInit() {
  lgModelMu = xSemaphoreCreateRecursiveMutex();
  // 7B HMI: SOLO zap — UI smí TX (bez wall controlleru)
  soloRezimTab5 = true;
  mCilova = 0;
  novaCilovaTeplota = 0;
}

void lgModelLock() {
  if (lgModelMu) { xSemaphoreTakeRecursive(lgModelMu, portMAX_DELAY); }
}

void lgModelUnlock() {
  if (lgModelMu) { xSemaphoreGiveRecursive(lgModelMu); }
}

void nastavStavovyText(const char* text) {
  lgModelLock();
  snprintf(posledniStavovyText, sizeof(posledniStavovyText), "%s", text);
  lgModelUnlock();
}

void lgModelSnapA0(const uint8_t* data, uint8_t len) {
  lgModelLock();
  lgModelSnapA0Locked(data, len);
  lgModelUnlock();
}

void lgModelSnapA0Locked(const uint8_t* data, uint8_t len) {
  if (len > sizeof(a0Snap)) {
    len = sizeof(a0Snap);
  }
  memcpy(a0Snap, data, len);
  a0SnapLen = len;
  s_casPosledniA0Ms = millis();
}

uint8_t lgModelA0Bajt(uint8_t idx, uint8_t vychozi) {
  lgModelLock();
  uint8_t v = (a0SnapLen > idx) ? a0Snap[idx] : vychozi;
  lgModelUnlock();
  return v;
}

bool lgMaCerstoA0(uint32_t maxAgeMs) {
  if (maxAgeMs == 0) {
    maxAgeMs = LG_A0_FRESH_MS;
  }
  lgModelLock();
  const unsigned long t = s_casPosledniA0Ms;
  lgModelUnlock();
  if (t == 0) {
    return false;
  }
  return (millis() - t) < maxAgeMs;
}
