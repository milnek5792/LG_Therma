// main.cpp — Waveshare 7B: display + EEZ UI + LIN UART2
#include <Arduino.h>
#include <USB.h>
#include <cstdarg>
#include <cstdio>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_private/brownout.h>

#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "i2c.h"
#include "io_extension.h"
#include "ui_bus_bindings.h"
#include "ui_eez_model.h"
#include "ui_net_sync.h"
#include "ui_ui_lvgl.h"

SET_LOOP_TASK_STACK_SIZE(48 * 1024);

static const char* TAG = "main";
static bool s_linStarted = false;
static uint32_t s_bootMs = 0;

// ESP_LOG → TinyUSB Serial (USB-C "USB"). UART0/43-44 necháváme pro LIN.
static int usbCdcLogVprintf(const char* fmt, va_list args) {
  char buf[256];
  const int n = vsnprintf(buf, sizeof(buf), fmt, args);
  if (n > 0) {
    const size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
    Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
  }
  return n;
}

static void boardEnableNativeUsbCdc() {
  DEV_I2C_Init();
  IO_EXTENSION_Init();  // EXIO5=0 → USB, ne CAN
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);
}

void setup() {
  esp_brownout_disable();

  // 1) Mux na USB (když je zapojený kabel do "USB")
  boardEnableNativeUsbCdc();
  delay(50);

  // 2) TinyUSB CDC — monitor jen když je kabel v USB-C "USB"
  USB.begin();
  Serial.begin(115200);
  Serial.setTxTimeoutMs(100);
  {
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 1500) {
      delay(10);
    }
  }
  esp_log_set_vprintf(usbCdcLogVprintf);
  delay(200);

  Serial.println();
  Serial.println("=== LG THERMA 7B boot (USB CDC) ===");
  Serial.flush();
  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "LG THERMA 7B + LIN");
  ESP_LOGI(TAG, "PSRAM size=%u free=%u",
           (unsigned)ESP.getPsramSize(),
           (unsigned)ESP.getFreePsram());
  ESP_LOGI(TAG, "Upload=CH343 (UART). Monitor=USB-C USB.");
  ESP_LOGI(TAG, "==================================================");

  if (ESP.getPsramSize() == 0) {
    ESP_LOGE(TAG, "FATAL: PSRAM=0");
    for (;;) {
      delay(1000);
    }
  }

  uiEezInit();
  uiNetInit();
  uiLvglInit();

  if (!uiLvglInitDone()) {
    ESP_LOGE(TAG, "LVGL init FAILED — hang");
    for (;;) {
      delay(1000);
    }
  }

  lgModelInit();
  s_bootMs = millis();
  ESP_LOGI(TAG, "ready — LIN start za %u ms (RX=%d TX=%d)",
           (unsigned)LG_LIN_START_DELAY_MS, LG_MBUS_RX_PIN, LG_MBUS_TX_PIN);
}

void loop() {
  const uint32_t now = millis();

#if LG_DEFER_LIN_START
  if (!s_linStarted && (now - s_bootMs) >= LG_LIN_START_DELAY_MS) {
    lgBusInit();
    s_linStarted = true;
    ESP_LOGI(TAG, "LIN started");
  }
#else
  if (!s_linStarted) {
    lgBusInit();
    s_linStarted = true;
  }
#endif

  if (s_linStarted) {
    lgBusTick();
    uiBusBindingsTick();
  }

  uiLvglTick();

  static uint32_t s_lastLoopHb = 0;
  if (now - s_lastLoopHb >= 2000) {
    s_lastLoopHb = now;
    // Přímý Serial — ať je vidět i když ESP_LOG selže
    Serial.printf("HB ms=%lu lin=%d pkts=%lu\r\n",
                  (unsigned long)now,
                  (int)s_linStarted,
                  s_linStarted ? lgPocetPaketu() : 0UL);
    Serial.flush();
    ESP_LOGI(TAG, "HB ms=%lu freeze=%d flush=%u lin_pkts=%lu rx=%lu",
             (unsigned long)now,
             (int)uiLvglIsFrozen(),
             (unsigned)uiLvglFlushCount(),
             s_linStarted ? lgPocetPaketu() : 0UL,
             s_linStarted ? lgPocetRxBajtu() : 0UL);
  }
  delay(3);
}
