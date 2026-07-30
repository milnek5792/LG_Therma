// ui_display_mgr.cpp — jas (NVS) + usínání backlightu, wake na GT911
#include "ui_display_mgr.h"

#include "rgb_lcd_port.h"
#include "storage_config_nvs.h"
#include "touch.h"

#include <Arduino.h>
#include <esp_log.h>

namespace {

static const char* TAG = "DISP";

uint8_t s_brightness = 60;
uint32_t s_timeoutSec = 120;
uint32_t s_lastActivityMs = 0;
bool s_asleep = false;
bool s_inited = false;
bool s_ignoreUntilRelease = false;
esp_lcd_touch_handle_t s_touchWake = nullptr;

constexpr uint32_t kSleepOptsSec[] = {0, 60, 120, 300, 600, 1800};
constexpr int kSleepOptCount = 6;

void applyBrightnessHw(uint8_t pct) {
  waveshare_rgb_lcd_set_brightness(pct);
}

void goSleep() {
  if (s_asleep) {
    return;
  }
  s_asleep = true;
  wavesahre_rgb_lcd_bl_off();
  ESP_LOGI(TAG, "sleep (timeout %lu s)", (unsigned long)s_timeoutSec);
}

}  // namespace

void uiDisplayInit(void) {
  storageInit();
  s_brightness = storageLoadBrightness();
  s_timeoutSec = storageLoadSleepTimeoutSec();
  applyBrightnessHw(s_brightness);
  s_lastActivityMs = millis();
  s_asleep = false;
  s_inited = true;
  ESP_LOGI(TAG, "init brightness=%u%% sleep=%lus", (unsigned)s_brightness,
           (unsigned long)s_timeoutSec);
}

void uiDisplayBindTouch(void* touch) {
  s_touchWake = static_cast<esp_lcd_touch_handle_t>(touch);
}

void uiDisplayNoteActivity(void) {
  s_lastActivityMs = millis();
}

bool uiDisplayIsAsleep(void) { return s_asleep; }

void uiDisplayWake(void) {
  if (!s_inited) {
    return;
  }
  if (s_asleep) {
    // Dvojí zápis — po sleep IO extender občas potřebuje znovu enable
    waveshare_rgb_lcd_set_brightness(s_brightness ? s_brightness : 60);
    delay(2);
    wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "wake brightness=%u%%", (unsigned)s_brightness);
    s_ignoreUntilRelease = true;
  }
  s_asleep = false;
  s_lastActivityMs = millis();
}

uint8_t uiDisplayGetBrightness(void) { return s_brightness; }

void uiDisplaySetBrightness(uint8_t percent, bool persist) {
  if (percent < 10) {
    percent = 10;
  }
  if (percent > 97) {
    percent = 97;
  }
  s_brightness = percent;
  if (!s_asleep) {
    applyBrightnessHw(percent);
  }
  if (persist) {
    storageSaveBrightness(percent);
  }
  uiDisplayNoteActivity();
}

uint32_t uiDisplayGetSleepTimeoutSec(void) { return s_timeoutSec; }

void uiDisplaySetSleepTimeoutSec(uint32_t sec, bool persist) {
  s_timeoutSec = sec;
  if (persist) {
    storageSaveSleepTimeoutSec(sec);
  }
  uiDisplayNoteActivity();
}

const char* uiDisplaySleepTimeoutLabel(void) {
  switch (s_timeoutSec) {
    case 0:
      return "Vypnuto";
    case 60:
      return "1 min";
    case 120:
      return "2 min";
    case 300:
      return "5 min";
    case 600:
      return "10 min";
    case 1800:
      return "30 min";
    default:
      return "?";
  }
}

void uiDisplayCycleSleepTimeout(void) {
  int idx = 0;
  for (int i = 0; i < kSleepOptCount; ++i) {
    if (kSleepOptsSec[i] == s_timeoutSec) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % kSleepOptCount;
  uiDisplaySetSleepTimeoutSec(kSleepOptsSec[idx], true);
  ESP_LOGI(TAG, "sleep timeout -> %lus (%s)", (unsigned long)s_timeoutSec,
           uiDisplaySleepTimeoutLabel());
}

bool uiDisplayHandleTouchWhileAsleep(bool pressed) {
  if (s_asleep) {
    if (pressed) {
      uiDisplayWake();
    }
    return true;
  }
  // Po probuzení spolknout stisk až do puštění — jinak omylem klikne UI
  if (s_ignoreUntilRelease) {
    if (!pressed) {
      s_ignoreUntilRelease = false;
    }
    return true;
  }
  return false;
}

void uiDisplayTick(void) {
  if (!s_inited) {
    return;
  }

  if (s_asleep) {
    if (s_touchWake) {
      esp_lcd_touch_read_data(s_touchWake);
      uint16_t x[1] = {0};
      uint16_t y[1] = {0};
      uint16_t s[1] = {0};
      uint8_t n = 0;
      const bool touched =
          esp_lcd_touch_get_coordinates(s_touchWake, x, y, s, &n, 1);
      if (touched || n > 0) {
        uiDisplayWake();
      }
    }
    return;
  }

  if (s_timeoutSec == 0) {
    return;
  }
  const uint32_t now = millis();
  if ((now - s_lastActivityMs) >= (s_timeoutSec * 1000UL)) {
    goSleep();
  }
}
