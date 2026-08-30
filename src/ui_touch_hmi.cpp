#include "ui_touch_hmi.h"

#include "ui_hmi_layout.h"

#include <Arduino.h>
#include <M5Unified.h>

namespace {

constexpr int kDebounceMs = 250;
constexpr unsigned long kFingerTimeoutMs = 2000;

bool s_fingerDown = false;
int s_downX = 0;
int s_downY = 0;
unsigned long s_downSinceMs = 0;
unsigned long s_lastActionMs = 0;
bool s_loggedFirst = false;

bool readTouch(int& x, int& y) {
  if (M5.Touch.getCount() <= 0) {
    return false;
  }

  const auto detail = M5.Touch.getDetail(0);
  if (!detail.isPressed()) {
    return false;
  }

  x = detail.x;
  y = detail.y;
  return true;
}

void fireClick(int x, int y) {
  if (millis() - s_lastActionMs < kDebounceMs) {
    return;
  }

  const HmiDotykBtn btn = hmiZjistiTlacitko(x, y);
  if (btn == HMI_DOTYK_ZADNY) {
    Serial.printf("[TOUCH] miss %d,%d\n", x, y);
    return;
  }

  s_lastActionMs = millis();
  Serial.printf("[TOUCH] %s @ %d,%d\n", hmiBtnName(btn), x, y);
  hmiProvedAkci(btn);
  if (btn != HMI_DOTYK_TICHY) {
    potrebaObnovitDisplej = true;
  }
}

}  // namespace

void uiTouchHmiSetupCheck() {
  Serial.printf("[setup] touch enabled=%d size=%dx%d\n",
                (int)M5.Touch.isEnabled(), M5.Display.width(), M5.Display.height());
}

void uiTouchHmiPoll() {
  int x = 0;
  int y = 0;

  if (readTouch(x, y)) {
    if (!s_loggedFirst) {
      s_loggedFirst = true;
      Serial.printf("[TOUCH] first @ %d,%d\n", x, y);
    }

    if (!s_fingerDown) {
      s_fingerDown = true;
      s_downX = x;
      s_downY = y;
      s_downSinceMs = millis();
    } else if (millis() - s_downSinceMs > kFingerTimeoutMs) {
      s_fingerDown = false;
    }
    return;
  }

  if (!s_fingerDown) {
    return;
  }

  s_fingerDown = false;
  fireClick(s_downX, s_downY);
}
