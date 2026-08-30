#include "ui_hmi_draw.h"

#include "bus_lg_model.h"
#include "ui_displej_hmi.h"

#include <Arduino.h>
#include <M5Unified.h>

namespace {

constexpr uint8_t kBootBrightness = 0;
constexpr uint8_t kNormalBrightness = 255;
constexpr int kPauseTicks = 25;

enum class Phase : uint8_t {
  kStatic = 0,
  kPause,
  kDynamic,
  kIdle,
};

Phase s_phase = Phase::kStatic;
int s_staticStep = 0;
int s_dynamicStep = 0;
int s_pauseTicks = 0;
bool s_uiReady = false;
bool s_linReady = false;

void flushDisplay() {
  M5.Display.display();
}

void applyNormalBrightness() {
  M5.Display.setBrightness(uiEez.sig_utlum ? 120 : kNormalBrightness);
}

}  // namespace

void uiHmiDrawInit() {
  M5.Display.setBrightness(kBootBrightness);
  M5.Display.fillScreen(TFT_BLACK);
  flushDisplay();

  s_staticStep = 0;
  s_dynamicStep = 0;
  s_pauseTicks = 0;
  s_uiReady = false;
  s_linReady = false;
  s_phase = Phase::kStatic;

  Serial.println("[UI] direct draw init (core 1, bez canvas)");
}

bool uiHmiDrawShouldStartUi() {
  if (s_uiReady) {
    s_uiReady = false;
    return true;
  }
  return false;
}

bool uiHmiDrawShouldStartLin() {
  if (s_linReady) {
    s_linReady = false;
    return true;
  }
  return false;
}

int uiHmiDrawLoopDelayMs() {
  switch (s_phase) {
    case Phase::kStatic:
      return 80;
    case Phase::kPause:
      return 100;
    case Phase::kDynamic:
      return 100;
    default:
      return 5;
  }
}

void uiHmiDrawTick() {
  switch (s_phase) {
    case Phase::kStatic:
      if (hmiStaticOk) {
        flushDisplay();
        s_phase = Phase::kPause;
        s_pauseTicks = 0;
        Serial.println("[UI] static flush");
        break;
      }
      if (hmiNakresliStaticKrok(s_staticStep)) {
        ++s_staticStep;
        break;
      }
      Serial.printf("[UI] static krok=%d done\n", s_staticStep);
      flushDisplay();
      s_phase = Phase::kPause;
      s_pauseTicks = 0;
      break;

    case Phase::kPause:
      if (++s_pauseTicks >= kPauseTicks) {
        s_dynamicStep = 0;
        s_phase = Phase::kDynamic;
        Serial.println("[UI] dynamic start");
      }
      break;

    case Phase::kDynamic:
      if (hmiNakresliDynamicKrok(s_dynamicStep)) {
        ++s_dynamicStep;
        flushDisplay();
        break;
      }
      applyNormalBrightness();
      hmiMarkRefreshDone();
      s_uiReady = true;
      s_linReady = true;
      s_phase = Phase::kIdle;
      Serial.printf("[UI] first paint done static=%d\n", (int)hmiStaticOk);
      break;

    case Phase::kIdle:
    default:
      if (!hmiJeCasNaObnovu(false)) {
        break;
      }
      uiEezSyncFromBus();
      hmiNakresliDynamicke();
      flushDisplay();
      hmiMarkRefreshDone();
      break;
  }
}
