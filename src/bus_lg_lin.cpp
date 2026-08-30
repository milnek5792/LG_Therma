#include "bus_lg_lin.h"
#include "bus_lg_config.h"
#include "net_sdio_arbiter.h"

static unsigned long s_rxBytes = 0;
static bool s_uartReady = false;
static unsigned long s_lastA0RxMs = 0;
static unsigned long s_a0PeriodMs = 0;

bool lgZapisBezi() { return lgZapisAktivni(); }

unsigned long lgA0PeriodMs() { return s_a0PeriodMs; }

unsigned long lgPocetPaketu() { return pocetPaketu; }

unsigned long lgPocetRxBajtu() { return s_rxBytes; }

bool lgBusIsReady() { return s_uartReady; }

void lgBusInit() {
  if (s_uartReady) { return; }
  // ESP32 HardwareSerial.begin(baud, config, rxPin, txPin)
  LG_Serial.begin(LG_BAUDRATE, SERIAL_8N1, LG_MBUS_RX_PIN, LG_MBUS_TX_PIN);
  s_uartReady = true;
  Serial.printf("[LIN] UART RX=%d TX=%d @ %u bps\n",
                LG_MBUS_RX_PIN, LG_MBUS_TX_PIN, (unsigned)LG_BAUDRATE);
}

void lgBusTick() {
  lgObsluhaCekaniOrig();
  lgZapisObsluha();

  while (LG_Serial.available() > 0) {
    uint8_t b = LG_Serial.read();
    ++s_rxBytes;
    posledniBajtMs = millis();
    if (indexLg < sizeof(bufferLg)) { bufferLg[indexLg++] = b; }
  }

  if (indexLg > 0
      && (indexLg >= sizeof(bufferLg) || (millis() - posledniBajtMs > 40))) {
    // Jen během TLS connect/handshake — ne při běžném MQTT
    if (monitorPozastaven || netSdioTlsBusy()) {
      odposlechSberniceTichy(bufferLg, indexLg);
    } else {
      odposlechSbernice(bufferLg, indexLg);
    }

    if (bufferLg[0] == 0xA0 && indexLg >= 14) {
      const unsigned long nowMs = millis();
      if (s_lastA0RxMs > 0) {
        s_a0PeriodMs = nowMs - s_lastA0RxMs;
      }
      s_lastA0RxMs = nowMs;

      lgModelLock();
      lgModelSnapA0(bufferLg, indexLg);
      lgAktualizujPosledniA0(bufferLg[2], bufferLg[3]);
      mVstupni = bufferLg[11];
      mVystupni = bufferLg[12];

      if (!pozadavekNaZapis) {
        mCilova = bufferLg[8];
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

    if (bufferLg[0] == 0xA0 && pozadavekNaZapis) {
      lgModelLock();
      const bool zmenaStartu = pozadavekZmenaStartu;
      const bool zapProZapis = zmenaStartu ? cilovyZapnutoTab5 : stavZapnuto;
      const uint8_t teplota = novaCilovaTeplota;
      pozadavekNaZapis = false;
      pozadavekZmenaStartu = false;
      if (zmenaStartu && zapProZapis) {
        lgZapisPovolenStart = true;
      }
      lgModelUnlock();
      provedZapisTeploty(teplota, zapProZapis, zmenaStartu);
    }

    indexLg = 0;
  }
}
