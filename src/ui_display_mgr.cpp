// ui_display_mgr.cpp — jas (NVS) + usínání backlightu, Tab5 M5Unified
#include "ui_display_mgr.h"

#include "storage_config_nvs.h"
#include "ui_display_bus.h"

#include <M5Unified.h>
#include <Arduino.h>

namespace {

uint8_t s_brightness = 60;
uint8_t s_persistedBrightness = 60;
uint32_t s_timeoutSec = 120;
uint32_t s_persistedSleepSec = 120;
uint32_t s_lastActivityMs = 0;
uint32_t s_brightnessPendingMs = 0;
bool s_brightnessPending = false;
bool s_asleep = false;
bool s_inited = false;
bool s_ignoreUntilRelease = false;

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
  Serial.printf("[DISP] sleep (timeout %lu s)\n", (unsigned long)s_timeoutSec);
}

}  // namespace

void uiDisplayInit(void) {
  storageInit();
  s_brightness = storageLoadBrightness();
  s_persistedBrightness = s_brightness;
  s_timeoutSec = storageLoadSleepTimeoutSec();
  s_persistedSleepSec = s_timeoutSec;
  s_brightnessPending = false;
  applyBrightnessHw(s_brightness);
  s_lastActivityMs = millis();
  s_asleep = false;
  s_inited = true;
  Serial.printf("[DISP] NVS jas=%u%% usinani=%lus (%s)\n",
                (unsigned)s_brightness, (unsigned long)s_timeoutSec,
                uiDisplaySleepTimeoutLabel());
}

void uiDisplayNoteActivity(void) {
  if (s_asleep) {
    return;
  }
  s_lastActivityMs = millis();
}

bool uiDisplayIsAsleep(void) { return s_asleep; }

void uiDisplayWake(void) {
  if (!s_inited) {
    return;
  }
  const bool wasAsleep = s_asleep;
  s_asleep = false;
  applyBrightnessHw(s_brightness ? s_brightness : 60);
  s_lastActivityMs = millis();
  if (wasAsleep) {
    s_ignoreUntilRelease = true;
    Serial.printf("[DISP] wake brightness=%u%%\n", (unsigned)s_brightness);
  }
}

bool uiDisplayHandleTouchWhileAsleep(bool pressed) {
  if (s_asleep) {
    if (pressed) {
      uiDisplayWake();
    }
    return true;
  }
  if (s_ignoreUntilRelease) {
    if (!pressed) {
      s_ignoreUntilRelease = false;
    }
    return true;
  }
  return false;
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
    s_persistedBrightness = percent;
    s_brightnessPending = false;
    Serial.printf("[DISP] NVS save jas=%u%%\n", (unsigned)percent);
  } else if (percent != s_persistedBrightness) {
    s_brightnessPending = true;
    s_brightnessPendingMs = millis();
  }
  uiDisplayNoteActivity();
}

uint32_t uiDisplayGetSleepTimeoutSec(void) { return s_timeoutSec; }

void uiDisplaySetSleepTimeoutSec(uint32_t sec, bool persist) {
  s_timeoutSec = sec;
  if (persist) {
    storageSaveSleepTimeoutSec(sec);
    s_persistedSleepSec = sec;
    Serial.printf("[DISP] NVS save usinani=%lus\n", (unsigned long)sec);
  }
  uiDisplayNoteActivity();
}

void uiDisplayFlushPendingStorage(void) {
  if (!s_inited) {
    return;
  }
  if (s_brightnessPending &&
      (millis() - s_brightnessPendingMs) >= 400) {
    if (s_brightness != s_persistedBrightness) {
      storageSaveBrightness(s_brightness);
      s_persistedBrightness = s_brightness;
      Serial.printf("[DISP] deferred NVS jas=%u%%\n", (unsigned)s_brightness);
    }
    s_brightnessPending = false;
  }
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
  if (!s_inited) {
    return;
  }

  if (s_asleep) {
    if (M5.Touch.getCount() > 0) {
      const auto detail = M5.Touch.getDetail(0);
      if (detail.isPressed()) {
        uiDisplayWake();
      }
    }
    return;
  }

  if (s_timeoutSec == 0) {
    return;
  }
  if ((millis() - s_lastActivityMs) >= (s_timeoutSec * 1000UL)) {
    goSleep();
  }
}
