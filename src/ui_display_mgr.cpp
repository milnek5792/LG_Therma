// ui_display_mgr.cpp — jas (NVS) + usínání backlightu, Tab5 M5Unified
#include "ui_display_mgr.h"

#include "storage_config_nvs.h"
#include "ui_display_bus.h"

#include <M5Unified.h>
#include <Arduino.h>

namespace {

uint8_t s_brightness = 60;
uint32_t s_timeoutSec = 120;
uint32_t s_lastActivityMs = 0;
bool s_asleep = false;
bool s_inited = false;

constexpr uint32_t kSleepOptsSec[] = {0, 60, 120, 300, 600, 1800};
constexpr int kSleepOptCount = 6;

uint8_t percentToHw(uint8_t percent) {
  if (percent < 10) {
    percent = 10;
  }
  if (percent > 97) {
    percent = 97;
  }
  return static_cast<uint8_t>((static_cast<unsigned>(percent) * 255u + 48u) / 97u);
}

void applyBrightnessHw(uint8_t percent) {
  // Backlight (M5IOE1) nejde přes SDIO panel — nevyžaduje display mutex.
  M5.Display.setBrightness(percentToHw(percent));
}

void goSleep() {
  if (s_asleep) {
    return;
  }
  s_asleep = true;
  if (uiDisplayBusLock(portMAX_DELAY)) {
    M5.Display.setBrightness(0);
    uiDisplayBusUnlock();
  }
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
  Serial.printf("[DISP] NVS jas=%u%% usinani=%lus (%s)\n",
                (unsigned)s_brightness, (unsigned long)s_timeoutSec,
                uiDisplaySleepTimeoutLabel());
}

void uiDisplayNoteActivity(void) {
  s_lastActivityMs = millis();
  if (s_asleep) {
    uiDisplayWake();
  }
}

bool uiDisplayIsAsleep(void) { return s_asleep; }

void uiDisplayWake(void) {
  if (!s_inited) {
    return;
  }
  s_asleep = false;
  applyBrightnessHw(s_brightness ? s_brightness : 60);
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
    Serial.printf("[DISP] NVS save jas=%u%%\n", (unsigned)percent);
  }
  uiDisplayNoteActivity();
}

uint32_t uiDisplayGetSleepTimeoutSec(void) { return s_timeoutSec; }

void uiDisplaySetSleepTimeoutSec(uint32_t sec, bool persist) {
  s_timeoutSec = sec;
  if (persist) {
    storageSaveSleepTimeoutSec(sec);
    Serial.printf("[DISP] NVS save usinani=%lus\n", (unsigned long)sec);
  }
  uiDisplayNoteActivity();
}

const char* uiDisplaySleepTimeoutLabel(void) {
  switch (s_timeoutSec) {
    case 0: return "Vypnuto";
    case 60: return "1 min";
    case 120: return "2 min";
    case 300: return "5 min";
    case 600: return "10 min";
    case 1800: return "30 min";
    default: return "?";
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
}

void uiDisplayTick(void) {
  if (!s_inited || s_asleep) {
    return;
  }
  if (s_timeoutSec == 0) {
    return;
  }
  if ((millis() - s_lastActivityMs) >= (s_timeoutSec * 1000UL)) {
    goSleep();
  }
}
