#include "bus_lg_lin.h"
#include "bus_lg_config.h"
#include "net_sdio_arbiter.h"
#include <esp_log.h>

static const char* TAG = "LIN";
static unsigned long s_rxBytes = 0;
static bool s_uartReady = false;

bool lgZapisBezi() { return lgZapisAktivni(); }

unsigned long lgPocetPaketu() { return pocetPaketu; }

unsigned long lgPocetRxBajtu() { return s_rxBytes; }

bool lgBusIsReady() { return s_uartReady; }

void lgBusInit() {
  if (s_uartReady) { return; }
  // ESP32 HardwareSerial.begin(baud, config, rxPin, txPin)
  LG_Serial.setRxBufferSize(256);
  LG_Serial.begin(LG_BAUDRATE, SERIAL_8N1, TAB5_MBUS_RX_PIN, TAB5_MBUS_TX_PIN);
  s_uartReady = true;
  ESP_LOGI(TAG, "UART%u RX=%d TX=%d @ %u bps",
           (unsigned)LG_UART_NUM, TAB5_MBUS_RX_PIN, TAB5_MBUS_TX_PIN,
           (unsigned)LG_BAUDRATE);
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
      lgModelLock();
      lgModelSnapA0(bufferLg, indexLg);
      lgAktualizujPosledniA0(bufferLg[2], bufferLg[3]);
      mVstupni = bufferLg[11];
      mVystupni = bufferLg[12];

      if (!pozadavekNaZapis) {
        // Živý SP z TČ (A0 B8) — UI ukazuje skutečnost, ne náš command
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
      // Držený ON z UI má přednost — potvrzení čerpadla může přijít se zpožděním
      bool zapProZapis = pozadavekZmenaStartu
                             ? cilovyZapnutoTab5
                             : (cilovyZapnutoTab5 || stavZapnuto);
      uint8_t teplota = novaCilovaTeplota;
      bool zmenaStartu = pozadavekZmenaStartu;
      pozadavekNaZapis = false;
      pozadavekZmenaStartu = false;
      lgModelUnlock();
      provedZapisTeploty(teplota, zapProZapis, zmenaStartu);
    }

    indexLg = 0;
  }
}
