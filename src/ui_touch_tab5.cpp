#include "ui_touch_tab5.h"

#include "net_sdio_arbiter.h"
#include "ui_display_mgr.h"
#include "ui_ui_lvgl.h"

#include "ui_eez_actions.h"
#include "ui_eez_nav.h"
#include "ui_eez_plan.h"
#include "ui_eez_regulator.h"
#include "ui_eez_screens.h"
#include "ui_eez_settings.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <stdio.h>

namespace {

enum TouchPhase : uint8_t {
  kPhaseIdle = 0,
  kPhaseDown,
  kPhaseHold,
  kPhaseUp,
};

struct TouchBtn {
  int x;
  int y;
  int w;
  int h;
  const char* name;
  void (*action)(lv_event_t*);
};

constexpr int kDebounceMs = 250;
constexpr int kHitPad = 12;
constexpr int kMaxDriftPx = 55;

// MAIN: custom hit-test (LVGL indev vypnutý kvůli SDIO).
constexpr TouchBtn kMainButtons[] = {
    {48, 120, 128, 128, "minus", action_akce_teplota_minus},
    {608, 120, 128, 128, "plus", action_akce_teplota_plus},
    {865, 90, 195, 60, "start", action_akce_start_stop},
    {1070, 90, 195, 60, "stop", action_akce_start_stop},
    {1024, 600, 256, 120, "menu", action_akce_menu},
};

TouchPhase s_phase = kPhaseIdle;
volatile uint32_t s_pollCount = 0;
int s_uiX = 0;
int s_uiY = 0;
int8_t s_uiBtnIdx = -1;

bool s_fingerDown = false;
int s_downX = 0;
int s_downY = 0;
int s_downBaseX = 0;
int s_downBaseY = 0;
int s_lastX = 0;
int s_lastY = 0;
int8_t s_downBtnIdx = -1;
lv_obj_t* s_downLvglObj = nullptr;
unsigned long s_lastActionMs = 0;
bool s_loggedFirstTouch = false;

bool inRect(int tx, int ty, const TouchBtn& b, int pad = kHitPad) {
  return tx >= (b.x - pad) && tx < (b.x + b.w + pad) && ty >= (b.y - pad)
      && ty < (b.y + b.h + pad);
}

bool pointInObj(lv_obj_t* obj, int tx, int ty, int pad = kHitPad) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
    return false;
  }
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return tx >= (a.x1 - pad) && tx <= (a.x2 + pad) && ty >= (a.y1 - pad)
      && ty <= (a.y2 + pad);
}

void noteUserActivity() {
  uiDisplayNoteActivity();
  netSdioClearUiFreeze();
  // Drž SDIO heavy mimo MAIN — MQTT reconnect jinak parkne UI.
  if (!uiIsMainScreen()) {
    netSdioBeginUiHeavy(15000);
  }
}

lv_obj_t* hitTestObjList(lv_obj_t* const* list, int n, int tx, int ty) {
  for (int i = 0; i < n; ++i) {
    if (pointInObj(list[i], tx, ty)) {
      return list[i];
    }
  }
  return nullptr;
}

lv_obj_t* hitTestSettings(int tx, int ty) {
  lv_obj_t* list[] = {
      settingsObj.btn_back,        settingsObj.btn_wifi_toggle,
      settingsObj.btn_wifi_connect, settingsObj.btn_wifi_edit,
      settingsObj.btn_mqtt_toggle, settingsObj.btn_mqtt_connect,
      settingsObj.btn_ble,         settingsObj.btn_mac,
      settingsObj.btn_meter1,        settingsObj.btn_meter2,
      settingsObj.btn_meter3,        settingsObj.btn_plan,
      settingsObj.btn_servis,      settingsObj.btn_sleep,
      settingsObj.slider_brightness,
  };
  return hitTestObjList(list, (int)(sizeof(list) / sizeof(list[0])), tx, ty);
}

lv_obj_t* hitTestPlan(int tx, int ty) {
  if (!planObj.screen) {
    return nullptr;
  }
  // Modal má prioritu
  if (planObj.modal_bg && !lv_obj_has_flag(planObj.modal_bg, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_t* modal[] = {
        planObj.modal_btn_hotovo, planObj.modal_btn_od_minus, planObj.modal_btn_od_plus,
        planObj.modal_btn_del_minus, planObj.modal_btn_del_plus, planObj.modal_panel,
        planObj.modal_bg,
    };
    return hitTestObjList(modal, (int)(sizeof(modal) / sizeof(modal[0])), tx, ty);
  }

  lv_obj_t* found = nullptr;
  lv_obj_t* top[] = {planObj.btn_back, planObj.btn_toggle};
  found = hitTestObjList(top, 2, tx, ty);
  if (found) {
    return found;
  }
  found = hitTestObjList(planObj.btn_cas_karta, 5, tx, ty);
  if (found) {
    return found;
  }
  for (int d = 0; d < 7; ++d) {
    found = hitTestObjList(planObj.btn_bunky[d], 5, tx, ty);
    if (found) {
      return found;
    }
  }
  return nullptr;
}

lv_obj_t* hitTestRegulator(int tx, int ty) {
  if (!regulatorObj.screen) {
    return nullptr;
  }
  lv_obj_t* list[] = {
      regulatorObj.btn_back,   regulatorObj.btn_mode,  regulatorObj.btn_ekv,
      regulatorObj.btn_save,   regulatorObj.btn_kp_m,  regulatorObj.btn_kp_p,
      regulatorObj.btn_ki_m,   regulatorObj.btn_ki_p,  regulatorObj.btn_kd_m,
      regulatorObj.btn_kd_p,   regulatorObj.btn_bias_m, regulatorObj.btn_bias_p,
      regulatorObj.btn_cold_m, regulatorObj.btn_cold_p, regulatorObj.btn_warm_m,
      regulatorObj.btn_warm_p,
  };
  return hitTestObjList(list, (int)(sizeof(list) / sizeof(list[0])), tx, ty);
}

lv_obj_t* hitTestDynamic(int tx, int ty) {
  if (uiIsSettingsScreen()) {
    return hitTestSettings(tx, ty);
  }
  if (uiIsPlanScreen()) {
    return hitTestPlan(tx, ty);
  }
  if (uiIsRegulatorScreen()) {
    return hitTestRegulator(tx, ty);
  }
  return nullptr;
}

lv_obj_t* mainButtonObject(int idx) {
  switch (idx) {
    case 0: return objects.btn_minus;
    case 1: return objects.btn_plus;
    case 2: return objects.btn_run;
    case 3: return objects.btn_stop;
    case 4: return objects.btn_menu;
    default: return nullptr;
  }
}

int buttonIndexMain(int tx, int ty) {
  const int count = (int)(sizeof(kMainButtons) / sizeof(kMainButtons[0]));
  for (int i = count - 1; i >= 0; --i) {
    lv_obj_t* obj = mainButtonObject(i);
    if (obj && pointInObj(obj, tx, ty)) {
      return i;
    }
    if (inRect(tx, ty, kMainButtons[i])) {
      return i;
    }
  }
  return -1;
}

int buttonIndexMainAt(int tx, int ty, int baseX, int baseY) {
  int idx = buttonIndexMain(tx, ty);
  if (idx >= 0) {
    return idx;
  }
  return buttonIndexMain(baseX, baseY);
}

void setPhase(TouchPhase phase, int x, int y, int8_t btnIdx) {
  s_phase = phase;
  s_uiX = x;
  s_uiY = y;
  s_uiBtnIdx = btnIdx;
}

bool readTouchPressed(int& x, int& y, int& baseX, int& baseY) {
  if (M5.Touch.getCount() <= 0) {
    return false;
  }
  const auto detail = M5.Touch.getDetail(0);
  if (!detail.isPressed()) {
    return false;
  }
  x = detail.x;
  y = detail.y;
  baseX = detail.base_x;
  baseY = detail.base_y;
  return true;
}

bool touchReleaseValidMain(int8_t btnIdx, int releaseX, int releaseY) {
  if (btnIdx < 0 || btnIdx >= (int)(sizeof(kMainButtons) / sizeof(kMainButtons[0]))) {
    return false;
  }
  lv_obj_t* obj = mainButtonObject(btnIdx);
  if (obj) {
    const bool downInBtn =
        pointInObj(obj, s_downX, s_downY) || pointInObj(obj, s_downBaseX, s_downBaseY);
    const bool relInBtn = pointInObj(obj, releaseX, releaseY);
    if (downInBtn || relInBtn) {
      if (abs(releaseX - s_downX) > kMaxDriftPx
          || abs(releaseY - s_downY) > kMaxDriftPx) {
        return false;
      }
      return true;
    }
  }
  const TouchBtn& btn = kMainButtons[btnIdx];
  const bool downInBtn =
      inRect(s_downX, s_downY, btn) || inRect(s_downBaseX, s_downBaseY, btn);
  const bool relInBtn = inRect(releaseX, releaseY, btn);
  if (!downInBtn && !relInBtn) {
    return false;
  }
  if (abs(releaseX - s_downX) > kMaxDriftPx
      || abs(releaseY - s_downY) > kMaxDriftPx) {
    return false;
  }
  return true;
}

bool touchReleaseValidDyn(lv_obj_t* obj, int releaseX, int releaseY) {
  if (!obj) {
    return false;
  }
  const bool downOk =
      pointInObj(obj, s_downX, s_downY) || pointInObj(obj, s_downBaseX, s_downBaseY);
  const bool relOk = pointInObj(obj, releaseX, releaseY);
  if (!downOk && !relOk) {
    return false;
  }
  if (abs(releaseX - s_downX) > kMaxDriftPx
      || abs(releaseY - s_downY) > kMaxDriftPx) {
    return false;
  }
  return true;
}

void applyPressedStyle(lv_obj_t* obj) {
  if (!obj) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x48484f), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
}

bool isBrightnessSlider(lv_obj_t* obj) {
  return obj == settingsObj.slider_brightness;
}

int sliderPercentFromX(lv_obj_t* slider, int touchX) {
  lv_area_t a;
  lv_obj_get_coords(slider, &a);
  const int w = a.x2 - a.x1;
  if (w <= 0) {
    return uiDisplayGetBrightness();
  }
  int pct = 10 + (touchX - a.x1) * 87 / w;
  if (pct < 10) {
    pct = 10;
  }
  if (pct > 97) {
    pct = 97;
  }
  return pct;
}

void applySliderBrightness(lv_obj_t* slider, int touchX, bool persist) {
  if (!slider) {
    return;
  }
  uiSettingsSetBrightnessFromTouch(static_cast<uint8_t>(sliderPercentFromX(slider, touchX)),
                                   persist);
}

void fireLvglClick(lv_obj_t* obj) {
  if (!obj) {
    return;
  }
  if (isBrightnessSlider(obj)) {
    applySliderBrightness(obj, s_lastX, true);
    return;
  }
  lv_obj_send_event(obj, LV_EVENT_CLICKED, nullptr);
}

}  // namespace

void uiTouchTab5Init() {}

void uiTouchVisualInit() {
  if (!uiIsMainScreen()) {
    return;
  }
  for (int i = 0; i < (int)(sizeof(kMainButtons) / sizeof(kMainButtons[0])); ++i) {
    lv_obj_t* obj = mainButtonObject(i);
    if (!obj) {
      continue;
    }
    if (i == 2 || i == 3) {
      continue;
    }
    applyPressedStyle(obj);
  }
}

void uiTouchVisualSync() {
  if (!uiIsMainScreen()) {
    return;
  }
  const int8_t idx =
      (s_phase == kPhaseDown || s_phase == kPhaseHold) ? s_downBtnIdx : -1;
  static int8_t lastApplied = -2;
  if (idx == lastApplied) {
    return;
  }
  if (lastApplied >= 0) {
    lv_obj_t* prev = mainButtonObject(lastApplied);
    if (prev) {
      lv_obj_remove_state(prev, LV_STATE_PRESSED);
      lv_obj_invalidate(prev);
    }
  }
  if (idx >= 0) {
    lv_obj_t* cur = mainButtonObject(idx);
    if (cur) {
      lv_obj_add_state(cur, LV_STATE_PRESSED);
      lv_obj_invalidate(cur);
    }
  }
  lastApplied = idx;
}

void uiTouchTab5SetupCheck() {
  Serial.printf("[setup] touch enabled=%d size=%dx%d\n",
                (int)M5.Touch.isEnabled(), M5.Display.width(),
                M5.Display.height());
}

bool uiTouchTab5Poll() {
  // Wi-Fi formulář: LVGL klávesnice. Ostatní = custom hit (funguje i při soft-freeze).
  if (uiLvglPointerInputEnabled()) {
    return false;
  }

  ++s_pollCount;

  int x = 0;
  int y = 0;
  int baseX = 0;
  int baseY = 0;

  if (readTouchPressed(x, y, baseX, baseY)) {
    if (!s_loggedFirstTouch) {
      s_loggedFirstTouch = true;
      Serial.printf("[TOUCH] first @ %d,%d base=%d,%d\n", x, y, baseX, baseY);
    }

    if (!s_fingerDown) {
      s_fingerDown = true;
      noteUserActivity();
      s_downX = x;
      s_downY = y;
      s_downBaseX = baseX;
      s_downBaseY = baseY;
      s_lastX = x;
      s_lastY = y;
      s_downLvglObj = nullptr;
      s_downBtnIdx = -1;

      if (uiIsMainScreen()) {
        s_downBtnIdx = (int8_t)buttonIndexMainAt(x, y, baseX, baseY);
      } else {
        s_downLvglObj = hitTestDynamic(x, y);
        if (!s_downLvglObj) {
          s_downLvglObj = hitTestDynamic(baseX, baseY);
        }
      }
      setPhase(kPhaseDown, x, y, s_downBtnIdx);
      Serial.printf("[TOUCH] down %d,%d base=%d,%d btn=%d dyn=%p\n", x, y, baseX,
                    baseY, (int)s_downBtnIdx, (void*)s_downLvglObj);
    } else {
      s_lastX = x;
      s_lastY = y;
      if (isBrightnessSlider(s_downLvglObj)) {
        applySliderBrightness(s_downLvglObj, x, false);
      }
      setPhase(kPhaseHold, x, y, s_downBtnIdx);
    }
    return false;
  }

  if (!s_fingerDown) {
    return false;
  }

  s_fingerDown = false;
  setPhase(kPhaseUp, s_downX, s_downY, s_downBtnIdx);
  Serial.printf("[TOUCH] up base=%d,%d btn=%d dyn=%p\n", s_downX, s_downY,
                (int)s_downBtnIdx, (void*)s_downLvglObj);

  const int8_t idx = s_downBtnIdx;
  lv_obj_t* dyn = s_downLvglObj;
  s_downBtnIdx = -1;
  s_downLvglObj = nullptr;

  // Jas: vždy uložit při puštění (před debounce — jinak NVS občas vypadne)
  if (dyn && isBrightnessSlider(dyn)) {
    applySliderBrightness(dyn, s_lastX, true);
    noteUserActivity();
    setPhase(kPhaseIdle, 0, 0, -1);
    return true;
  }

  if (millis() - s_lastActionMs < kDebounceMs) {
    setPhase(kPhaseIdle, 0, 0, -1);
    return false;
  }

  if (dyn) {
    if (!isBrightnessSlider(dyn) &&
        !touchReleaseValidDyn(dyn, s_lastX, s_lastY)) {
      Serial.printf("[TOUCH] dyn drift/miss @ %d,%d\n", s_downX, s_downY);
      setPhase(kPhaseIdle, 0, 0, -1);
      return false;
    }
    s_lastActionMs = millis();
    noteUserActivity();
    Serial.printf("[TOUCH] dyn click @ %d,%d\n", s_downX, s_downY);
    fireLvglClick(dyn);
    setPhase(kPhaseIdle, 0, 0, -1);
    return true;
  }

  if (idx < 0) {
    Serial.printf("[TOUCH] miss base=%d,%d\n", s_downX, s_downY);
    setPhase(kPhaseIdle, 0, 0, -1);
    return false;
  }

  if (!touchReleaseValidMain(idx, s_lastX, s_lastY)) {
    Serial.printf("[TOUCH] drift/miss btn=%d base=%d,%d rel=%d,%d\n", (int)idx,
                  s_downX, s_downY, s_lastX, s_lastY);
    setPhase(kPhaseIdle, 0, 0, -1);
    return false;
  }

  s_lastActionMs = millis();
  noteUserActivity();
  Serial.printf("[TOUCH] %s @ %d,%d\n", kMainButtons[idx].name, s_downX, s_downY);
  if (kMainButtons[idx].action) {
    kMainButtons[idx].action(nullptr);
  }
  setPhase(kPhaseIdle, 0, 0, -1);
  return true;
}

uint32_t uiTouchTab5PollCount() { return s_pollCount; }
