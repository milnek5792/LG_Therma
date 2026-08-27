// main.cpp — Waveshare 7B: LIN task + Arduino loop (LVGL + ctrl)
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_private/brownout.h>

#include "app_version.h"
#include "app_cmd.h"
#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"
#include "bus_lg_model.h"
#include "bus_lg_task.h"
#include "i2c.h"
#include "io_extension.h"
#include "ui_bus_bindings.h"
#include "ui_eez_model.h"
#include "ui_net_sync.h"
#include "ui_ui_lvgl.h"

SET_LOOP_TASK_STACK_SIZE(48 * 1024);

static const char* TAG = "main";

static int usbLogVprintf(const char* fmt, va_list args) {
  char buf[256];
  const int n = vsnprintf(buf, sizeof(buf), fmt, args);
  if (n > 0) {
    const size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
    Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
  }
  return n;
}

static void boardSelectNativeUsb() {
  DEV_I2C_Init();
  IO_EXTENSION_Init();
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);
}

void setup() {
  esp_brownout_disable();

  boardSelectNativeUsb();
  delay(100);

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(300);
  esp_log_set_vprintf(usbLogVprintf);

  Serial.println();
  Serial.printf("=== LG THERMA 7B FW %s ===\n", APP_FW_VERSION);
  ESP_LOGI(TAG, "LG THERMA 7B FW %s", APP_FW_VERSION);
  ESP_LOGI(TAG, "PSRAM size=%u free=%u",
           (unsigned)ESP.getPsramSize(),
           (unsigned)ESP.getFreePsram());

  if (ESP.getPsramSize() == 0) {
    ESP_LOGE(TAG, "FATAL: PSRAM=0");
    for (;;) {
      delay(1000);
    }
  }
  heap_caps_malloc_extmem_enable(8192);

  uiEezInit();
  appCmdInit();

  // LIN dřív než LVGL: prio 12 na core 1 naslouchá během pomalého display init.
  // Jinak se promešká A0 a při VYP TČ čekáme další cyklus (~10–25 s).
  lgModelInit();
  lgModelRestoreSessionFromNvs();
  lgBusStartTask();

  uiNetInit();
  uiLvglInit();

  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);

  if (!uiLvglInitDone()) {
    ESP_LOGE(TAG, "LVGL init FAILED — hang");
    for (;;) {
      delay(1000);
    }
  }

  ESP_LOGI(TAG, "ready — LIN prio=%u defer=%u ms, LVGL+ctrl in loop",
           (unsigned)LG_LIN_TASK_PRIO, (unsigned)LG_LIN_START_DELAY_MS);
}

void loop() {
  const uint32_t now = millis();

  // LVGL/VSYNC první — LIN/NVS nesmí blokovat flush
  uiLvglTick();

  if (lgBusIsReady()) {
    uiBusBindingsTick();
    uiBusFlushDeferredStorage();
  } else {
    appCmdDrainCtrl();
  }

  static uint32_t s_lastLoopHb = 0;
  if (now - s_lastLoopHb >= 2000) {
    s_lastLoopHb = now;
    IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);
    const bool linUp = lgBusIsReady();
    Serial.printf("HB ms=%lu lin=%d pkts=%lu\r\n",
                  (unsigned long)now,
                  (int)linUp,
                  linUp ? lgPocetPaketu() : 0UL);
    ESP_LOGI(TAG, "HB freeze=%d lite=%d q=%u flush=%u rx=%lu linTask=%d",
             (int)uiLvglIsFrozen(),
             (int)uiLvglIsDisplayLite(),
             (unsigned)appCmdQueueWaiting(),
             (unsigned)uiLvglFlushCount(),
             linUp ? lgPocetRxBajtu() : 0UL,
             (int)lgBusTaskRunning());
  }
  delay(3);
}
