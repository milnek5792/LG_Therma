#include "bus_lg_lin.h"
#include "bus_lg_config.h"
#include "net_sdio_arbiter.h"
#include <esp_log.h>

static const char* TAG = "LIN";
static unsigned long s_rxBytes = 0;
static bool s_uartReady = false;

/** +/- sync: čekat na další A0 jen když sběrnice běží rychle (~800 ms). */
static unsigned long s_spReqMs = 0;
static unsigned long s_lastA0RxMs = 0;
static unsigned long s_a0PeriodMs = 0;
static bool s_a0SinceSpReq = false;

constexpr unsigned long kSpSyncFastA0Ms = 2500;   // pod tím = rychlá sběrnice
constexpr unsigned long kSpSyncA0MaxWaitMs = 900; // max čekání ve slotu za A0

bool lgZapisBezi() { return lgZapisAktivni(); }

bool lgBusSmiPosilatTx(void) { return lgSmiPosilatTx(); }

unsigned long lgPocetPaketu() { return pocetPaketu; }

unsigned long lgPocetRxBajtu() { return s_rxBytes; }

bool lgBusIsReady() { return s_uartReady; }

unsigned long lgA0PeriodMs() { return s_a0PeriodMs; }

void lgBusInit() {
  if (s_uartReady) { return; }
  LG_Serial.setRxBufferSize(256);
  LG_Serial.begin(LG_BAUDRATE, SERIAL_8N1, TAB5_MBUS_RX_PIN, TAB5_MBUS_TX_PIN);
  s_uartReady = true;
  ESP_LOGI(TAG, "UART%u RX=%d TX=%d @ %u bps",
           (unsigned)LG_UART_NUM, TAB5_MBUS_RX_PIN, TAB5_MBUS_TX_PIN,
           (unsigned)LG_BAUDRATE);
}

/** Jediné místo pro LIN TX — voláno jen z linTask (bez race s lgZapis). */
static void lgObsluhaPozadavekNaZapis() {
  lgModelLock();
  if (!pozadavekNaZapis) {
    s_spReqMs = 0;
    s_a0SinceSpReq = false;
    lgModelUnlock();
    return;
  }
  if (!lgSmiPosilatTx()) {
    lgModelUnlock();
    return;
  }

  const bool zmenaStartu = pozadavekZmenaStartu;
  if (lgZapisAktivni() && lgZapis.jeStartSekvence && zmenaStartu) {
    lgModelUnlock();
    return;
  }

  // +/-: ve slotu za A0 jen při rychlé sběrnici; jinak C0 hned (TČ OFF ≈ 10–20 s)
  if (!zmenaStartu) {
    if (s_spReqMs == 0) {
      s_spReqMs = millis();
      s_a0SinceSpReq = false;
    }
    const unsigned long ageA0 =
        s_lastA0RxMs ? (millis() - s_lastA0RxMs) : 999999UL;
    const bool rychlaSbernice =
        s_a0PeriodMs > 0 && s_a0PeriodMs < kSpSyncFastA0Ms;
    const unsigned long slotMs =
        rychlaSbernice ? (s_a0PeriodMs / 2 + 150) : 0;
    const bool cekatNaA0 =
        rychlaSbernice && s_lastA0RxMs != 0 && ageA0 < slotMs;

    if (cekatNaA0 && !s_a0SinceSpReq &&
        (millis() - s_spReqMs) < kSpSyncA0MaxWaitMs) {
      lgModelUnlock();
      return;
    }
    if (s_a0SinceSpReq) {
      ESP_LOGI(TAG, "SP sync C0 po A0 (+%lu ms, period=%lu ms)",
               (unsigned long)(millis() - s_spReqMs),
               (unsigned long)s_a0PeriodMs);
    } else if (cekatNaA0) {
      ESP_LOGW(TAG, "SP sync timeout (+%lu ms, period=%lu ms)",
               (unsigned long)(millis() - s_spReqMs),
               (unsigned long)s_a0PeriodMs);
    } else {
      ESP_LOGI(TAG, "SP okamzite C0 (A0 pred %lu ms, period=%lu ms)",
               (unsigned long)ageA0, (unsigned long)s_a0PeriodMs);
    }
  }

  const bool zapProZapis = zmenaStartu
                               ? cilovyZapnutoTab5
                               : (cilovyZapnutoTab5 || stavZapnuto);
  const uint8_t teplota = novaCilovaTeplota;
  pozadavekNaZapis = false;
  pozadavekZmenaStartu = false;
  s_spReqMs = 0;
  s_a0SinceSpReq = false;
  lgModelUnlock();

  if (lgZapisAktivni()) {
    lgZapis.aktivni = false;
  }
  provedZapisTeploty(teplota, zapProZapis, zmenaStartu);
  lgZapisObsluha();
}

void lgBusTick() {
  lgObsluhaCekaniOrig();

  while (LG_Serial.available() > 0) {
    uint8_t b = LG_Serial.read();
    ++s_rxBytes;
    posledniBajtMs = millis();
    if (indexLg < sizeof(bufferLg)) { bufferLg[indexLg++] = b; }
  }

  if (indexLg > 0
      && (indexLg >= sizeof(bufferLg) || (millis() - posledniBajtMs > 40))) {
    if (monitorPozastaven || netSdioTlsBusy()) {
      odposlechSberniceTichy(bufferLg, indexLg);
    } else {
      odposlechSbernice(bufferLg, indexLg);
    }

    if (bufferLg[0] == 0xA0 && indexLg >= 14) {
      const unsigned long nowMs = millis();
      if (s_lastA0RxMs != 0) {
        const unsigned long dt = nowMs - s_lastA0RxMs;
        if (dt >= 300 && dt <= 60000) {
          s_a0PeriodMs =
              (s_a0PeriodMs == 0) ? dt : (s_a0PeriodMs * 3 + dt) / 4;
        }
      }
      s_lastA0RxMs = nowMs;

      lgModelLock();
      if (pozadavekNaZapis && s_spReqMs != 0 && !pozadavekZmenaStartu) {
        s_a0SinceSpReq = true;
      }
      lgModelSnapA0Locked(bufferLg, indexLg);
      lgAktualizujPosledniA0(bufferLg[2], bufferLg[3]);
      mVstupni = bufferLg[11];
      mVystupni = bufferLg[12];

      if (!pozadavekNaZapis && !lgZapisAktivni()) {
        mCilova = bufferLg[8];
        stavZapnuto = lgTcBeziNaSbernici(bufferLg[2], bufferLg[3]);
      } else if (!pozadavekNaZapis) {
        stavZapnuto = lgTcBeziNaSbernici(bufferLg[2], bufferLg[3]);
      }

      bliknuti = !bliknuti;
      char a0StavovyText[64];
      snprintf(a0StavovyText, sizeof(a0StavovyText), "A0 LIVE [%s] #%lu%s%s%s%s",
               bliknuti ? "*" : " ", lgPocetPaketu(),
               monitorPozastaven ? " PAUZA" : "",
               soloRezimTab5 ? " SOLO" : "",
               parallelRezimTab5 ? " PARALLEL" : "",
               cekameNaOrigStart ? " ORIG?" : (origOvladacDetekovan ? " ORIG!" : ""));
      snprintf(posledniStavovyText, sizeof(posledniStavovyText), "%s", a0StavovyText);
      potrebaObnovitDisplej = true;
      lgModelUnlock();
    }

    indexLg = 0;
  }

  lgObsluhaPozadavekNaZapis();
  lgZapisObsluha();
}
