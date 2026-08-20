// main.cpp — Waveshare 7B: LIN na UART header + logy na USB-C "USB"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_private/brownout.h>

#include "app_version.h"
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

// ESP_LOG default = UART0 (43/44) = LIN piny. Přesměrovat na USB CDC.
static int usbLogVprintf(const char* fmt, va_list args) {
  char buf[256];
  const int n = vsnprintf(buf, sizeof(buf), fmt, args);
  if (n > 0) {
    const size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
    Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
  }
  return n;
}

/** Waveshare: EXIO5 LOW = USB-C "USB" → ESP native USB; HIGH = CAN. */
static void boardSelectNativeUsb() {
  DEV_I2C_Init();
  IO_EXTENSION_Init();  // už nastaví EXIO5=0
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);
}

void setup() {
  esp_brownout_disable();

  // 1) Nejdřív mux USB (jinak Windows na "USB" nic nevidí — default je CAN)
  boardSelectNativeUsb();
  delay(100);

  // 2) HWCDC na USB-C "USB" (MODE=1). Nečekat na hosta — jinak boot visí.
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // neblokovat, když monitor ještě není
  delay(300);
  esp_log_set_vprintf(usbLogVprintf);

  Serial.println();
  Serial.printf("=== LG THERMA 7B FW %s ===\n", APP_FW_VERSION);
  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "LG THERMA 7B FW %s (no cmd/quiet)", APP_FW_VERSION);
  ESP_LOGI(TAG, "PSRAM size=%u free=%u",
           (unsigned)ESP.getPsramSize(),
           (unsigned)ESP.getFreePsram());
  ESP_LOGI(TAG, "Upload:  USB-C UART + DIP UART1 (CH343)");
  ESP_LOGI(TAG, "LIN:     UART header + DIP UART2");
  ESP_LOGI(TAG, "Monitor: USB-C USB COM (HWCDC), EXIO5=USB");
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

  // Po LVGL init znovu USB — IO_EXTENSION_Init v display path nesmí nechat CAN
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);

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
    // Držet mux na USB (kdyby něco přepsalo EXIO)
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);
    Serial.printf("HB ms=%lu lin=%d pkts=%lu\r\n",
                  (unsigned long)now,
                  (int)s_linStarted,
                  s_linStarted ? lgPocetPaketu() : 0UL);
    ESP_LOGI(TAG, "HB freeze=%d flush=%u rx=%lu",
             (int)uiLvglIsFrozen(),
             (unsigned)uiLvglFlushCount(),
             s_linStarted ? lgPocetRxBajtu() : 0UL);
  }
  delay(3);
}
