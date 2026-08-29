// ui_lvgl_port.cpp — LVGL 9 direct mode do RGB double-FB (anti-tear)
#include "ui_ui_lvgl.h"

#include "board_7b.h"
#include "gt911.h"
#include "i2c.h"
#include "io_extension.h"
#include "rgb_lcd_port.h"
#include "touch.h"
#include "ui_display_mgr.h"
#include "ui_eez_ntp_label.h"
#include "ui_eez_signal_leds.h"
#include "ui_net_sync.h"
#include "climate_ble_room.h"
#include "net_wifi_mgr.h"
#include "ui_eez_ui.h"
#include "ui_panel_scale.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstring>

namespace {

esp_lcd_panel_handle_t s_panel = nullptr;
esp_lcd_touch_handle_t s_touch = nullptr;
lv_display_t* s_disp = nullptr;
lv_indev_t* s_indev = nullptr;
void* s_fb0 = nullptr;
void* s_fb1 = nullptr;
size_t s_fbBytes = 0;

SemaphoreHandle_t s_vsyncSem = nullptr;
uint32_t s_lastTickMs = 0;
uint32_t s_flushCount = 0;
uint32_t s_rgbRestartCount = 0;
volatile bool s_initDone = false;
uint32_t s_healthGraceUntilMs = 0;
volatile bool s_frozen = false;
bool s_displayLite = false;
bool s_pointerInput = true;
bool s_touchArmed = false;
int s_horRes = 0;
int s_verRes = 0;
int16_t s_touchLastX = 0;
int16_t s_touchLastY = 0;

volatile uint32_t s_lastVsyncMs = 0;
volatile uint8_t s_vsyncMissStreak = 0;
volatile bool s_vsyncHealthFail = false;

bool s_recoverPending = false;
bool s_recoverHard = false;
uint32_t s_recoverDueMs = 0;
const char* s_recoverReason = nullptr;
bool s_recoverFollowHard = false;
uint32_t s_recoverFollowDueMs = 0;
const char* s_recoverFollowReason = nullptr;
uint32_t s_lastHardRecoverMs = 0;
uint32_t s_frozenSinceMs = 0;
bool s_wasAsleep = false;
bool s_wasFrozen = false;

/** Cross-core požadavky — aplikuje jen uiLvglTick (core 1). */
std::atomic<int> s_liteRef{0};
std::atomic<int> s_freezeRef{0};
std::atomic<uint8_t> s_reqSoftRecover{0};
std::atomic<uint8_t> s_reqHardRecover{0};
bool s_appliedLite = false;
bool s_appliedFreeze = false;

constexpr uint32_t kHardRecoverCooldownMs = 900;

void bumpRef(std::atomic<int>& ref, bool on) {
  if (on) {
    ref.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  int prev = ref.load(std::memory_order_relaxed);
  while (prev > 0) {
    if (ref.compare_exchange_weak(prev, prev - 1, std::memory_order_relaxed)) {
      return;
    }
  }
}

void applyDisplayReqs(void) {
  if (!s_initDone) {
    return;
  }

  const bool wantLite = s_liteRef.load(std::memory_order_relaxed) > 0;
  const bool wantFreeze = s_freezeRef.load(std::memory_order_relaxed) > 0;

  if (wantLite != s_appliedLite) {
    const bool wasLite = s_appliedLite;
    s_appliedLite = wantLite;
    s_displayLite = wantLite;
    if (wasLite && !wantLite && !uiDisplayIsAsleep()) {
      uiLvglScheduleRecover("lite_off", false, 80);
      uiLvglScheduleRecover("lite_off_hard", true, 420);
    }
  }

  if (wantFreeze != s_appliedFreeze) {
    s_appliedFreeze = wantFreeze;
    if (wantFreeze) {
      if (!s_frozen) {
        s_frozen = true;
        s_frozenSinceMs = millis();
        s_wasFrozen = true;
      }
    } else if (s_frozen) {
      s_frozen = false;
      s_frozenSinceMs = 0;
      if (s_wasFrozen) {
        uiLvglScheduleRecover("unfreeze", false, 80);
        uiLvglScheduleRecover("unfreeze_hard", true, 420);
      }
      s_wasFrozen = false;
    }
  }

  if (s_reqHardRecover.exchange(0, std::memory_order_relaxed) != 0) {
    uiLvglScheduleRecover("req_hard", true, 20);
  }
  if (s_reqSoftRecover.exchange(0, std::memory_order_relaxed) != 0) {
    uiLvglScheduleRecover("req_soft", false, 20);
  }
}

bool onVsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t* edata, void* user_ctx) {
  (void)panel;
  (void)edata;
  (void)user_ctx;
  s_lastVsyncMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  BaseType_t hp = pdFALSE;
  if (s_vsyncSem) {
    xSemaphoreGiveFromISR(s_vsyncSem, &hp);
  }
  return hp == pdTRUE;
}

void dispFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  (void)area;
  esp_lcd_panel_handle_t panel =
      static_cast<esp_lcd_panel_handle_t>(lv_display_get_user_data(disp));

  // DIRECT/FULL zero-copy: pxMap = adresu RGB framebufferu.
  // Bez draw_bitmap panel nepřepne buffer → tear / problikávání.
  if (panel && lv_display_flush_is_last(disp) && pxMap) {
    esp_lcd_panel_draw_bitmap(
        panel, 0, 0, BOARD_PANEL_W, BOARD_PANEL_H, pxMap);
    if (s_initDone && s_vsyncSem) {
      xSemaphoreTake(s_vsyncSem, 0);
      // Čekej na VSYNC (max ~3 snímky @ ~50 Hz)
      if (xSemaphoreTake(s_vsyncSem, pdMS_TO_TICKS(100)) != pdTRUE) {
        uint8_t miss = s_vsyncMissStreak;
        if (miss < 255) {
          ++miss;
        }
        s_vsyncMissStreak = miss;
        if (miss >= 2) {
          s_vsyncHealthFail = true;
          s_vsyncMissStreak = 0;
        }
      } else {
        s_vsyncMissStreak = 0;
      }
    }
  }

  ++s_flushCount;
  lv_display_flush_ready(disp);
}

void touchDrain() {
  if (!s_touch) {
    return;
  }
  for (int i = 0; i < 8; ++i) {
    esp_lcd_touch_read_data(s_touch);
    uint16_t x[1], y[1], s[1];
    uint8_t n = 0;
    esp_lcd_touch_get_coordinates(s_touch, x, y, s, &n, 1);
  }
}

void softRepaint(void) {
  lv_display_t* disp = lv_display_get_default();
  lv_obj_t* scr = lv_screen_active();
  if (scr) {
    lv_obj_invalidate(scr);
  }
  if (!disp) {
    return;
  }
  // Dva refr — přímý double-FB, ať se překreslí oba buffery
  lv_refr_now(disp);
  if (scr) {
    lv_obj_invalidate(scr);
  }
  lv_refr_now(disp);
}

void resetTouchInput(void) {
  touchDrain();
  if (s_indev) {
    lv_indev_reset(s_indev, nullptr);
  }
}

void doRgbRestart(const char* reason) {
  if (!s_panel) {
    return;
  }
  const esp_err_t err = esp_lcd_rgb_panel_restart(s_panel);
  ++s_rgbRestartCount;
  s_lastHardRecoverMs = millis();
  s_vsyncMissStreak = 0;
  s_vsyncHealthFail = false;
  ESP_LOGI("LVGL", "RGB DMA restart #%u reason=%s err=%s",
           (unsigned)s_rgbRestartCount,
           reason ? reason : "?",
           esp_err_to_name(err));
}

void runScheduledRecover(void) {
  const uint32_t now = millis();

  // Follow-up hard po softu (soft → hard žebřík)
  if (!s_recoverPending && s_recoverFollowHard && now >= s_recoverFollowDueMs) {
    s_recoverPending = true;
    s_recoverHard = true;
    s_recoverReason = s_recoverFollowReason ? s_recoverFollowReason : "follow_hard";
    s_recoverDueMs = now;
    s_recoverFollowHard = false;
  }

  if (!s_recoverPending || now < s_recoverDueMs) {
    return;
  }
  if (uiDisplayIsAsleep()) {
    // Ve spánku nepřekreslovat — recover po wake
    return;
  }

  const bool hard = s_recoverHard;
  const char* reason = s_recoverReason ? s_recoverReason : "sched";
  s_recoverPending = false;
  s_recoverHard = false;

  if (s_frozen) {
    s_frozen = false;
    s_frozenSinceMs = 0;
    s_wasFrozen = false;
  }

  resetTouchInput();

  if (hard) {
    const bool forceRestart =
        reason && (strcmp(reason, "vsync_miss") == 0 ||
                   strcmp(reason, "vsync_silent") == 0 ||
                   strcmp(reason, "ble_lite_off_hard") == 0 ||
                   strcmp(reason, "unfreeze_hard") == 0 ||
                   strcmp(reason, "wake_hard") == 0 ||
                   strcmp(reason, "freeze_wd") == 0);
    if (!forceRestart && s_lastHardRecoverMs != 0 &&
        (now - s_lastHardRecoverMs) < kHardRecoverCooldownMs) {
      softRepaint();
    } else {
      doRgbRestart(reason);
      softRepaint();
    }
  } else {
    softRepaint();
  }

  resetTouchInput();
}

void displayHealthWatchdog(void) {
  if (!s_initDone || !s_touchArmed || uiDisplayIsAsleep()) {
    return;
  }

  const uint32_t now = millis();

  if (now < s_healthGraceUntilMs) {
    return;
  }

  if (s_frozen && s_frozenSinceMs != 0 && (now - s_frozenSinceMs) > 4000) {
    // Během BLE scanu nehard-restartuj RGB — touch umře a heap je nízko
    if (climateBleIsBusy()) {
      s_frozen = false;
      s_frozenSinceMs = 0;
      return;
    }
    ESP_LOGW("LVGL", "freeze watchdog — unfreeze after %lu ms",
             (unsigned long)(now - s_frozenSinceMs));
    s_frozen = false;
    s_frozenSinceMs = 0;
    s_recoverPending = true;
    s_recoverHard = true;
    s_recoverReason = "freeze_wd";
    s_recoverDueMs = now + 30;
    return;
  }

  if (s_frozen) {
    return;
  }

  if (s_vsyncHealthFail) {
    s_vsyncHealthFail = false;
    ESP_LOGW("LVGL", "vsync miss streak — hard recover");
    s_recoverPending = true;
    s_recoverHard = true;
    s_recoverReason = "vsync_miss";
    s_recoverDueMs = now + 20;
    return;
  }

  const uint32_t lastVs = s_lastVsyncMs;
  if (lastVs != 0 && (now - lastVs) > 200) {
    ESP_LOGW("LVGL", "vsync silent %lu ms — hard recover",
             (unsigned long)(now - lastVs));
    s_recoverPending = true;
    s_recoverHard = true;
    s_recoverReason = "vsync_silent";
    s_recoverDueMs = now + 20;
  }
}

void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;

  if (!s_touch || !s_pointerInput || !s_touchArmed) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = s_touchLastX;
    data->point.y = s_touchLastY;
    return;
  }

  esp_lcd_touch_read_data(s_touch);
  uint16_t x[1] = {0};
  uint16_t y[1] = {0};
  uint16_t strength[1] = {0};
  uint8_t count = 0;
  const bool pressed =
      esp_lcd_touch_get_coordinates(s_touch, x, y, strength, &count, 1) && count > 0;

  if (pressed) {
    int16_t px = (int16_t)x[0];
    int16_t py = (int16_t)y[0];
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= BOARD_PANEL_W) px = BOARD_PANEL_W - 1;
    if (py >= BOARD_PANEL_H) py = BOARD_PANEL_H - 1;
    s_touchLastX = px;
    s_touchLastY = py;
    if (uiDisplayHandleTouchWhileAsleep(true)) {
      data->state = LV_INDEV_STATE_RELEASED;
    } else {
      data->state = LV_INDEV_STATE_PRESSED;
      uiDisplayNoteActivity();
    }
  } else {
    (void)uiDisplayHandleTouchWhileAsleep(false);
    data->state = LV_INDEV_STATE_RELEASED;
  }
  data->point.x = s_touchLastX;
  data->point.y = s_touchLastY;
}

}  // namespace

void uiLvglInit() {
  ESP_LOGI("LVGL", "direct-FB bring-up %dx%d", BOARD_PANEL_W, BOARD_PANEL_H);

  DEV_I2C_Init();
  IO_EXTENSION_Init();
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 0);  // držet native USB (ne CAN)
  IO_EXTENSION_Output(IO_EXTENSION_IO_3, 0);
  delay(20);
  IO_EXTENSION_Output(IO_EXTENSION_IO_3, 1);
  delay(20);

  s_panel = waveshare_esp32_s3_rgb_lcd_init();
  if (!s_panel) {
    ESP_LOGE("LVGL", "RGB LCD init FAILED");
    return;
  }
  uiDisplayInit();

  waveshare_get_frame_buffer(&s_fb0, &s_fb1);
  if (!s_fb0 || !s_fb1) {
    ESP_LOGE("LVGL", "framebuffers missing");
    return;
  }
  s_fbBytes = (size_t)BOARD_PANEL_W * (size_t)BOARD_PANEL_H * sizeof(uint16_t);
  memset(s_fb0, 0x10, s_fbBytes);
  memset(s_fb1, 0x10, s_fbBytes);

  s_vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(s_panel, &cbs, nullptr));

#if LG_THERMA_ENABLE_TOUCH
  s_touch = touch_gt911_init();
  touchDrain();
  uiDisplayBindTouch(s_touch);
#else
  s_touch = nullptr;
#endif

  lv_init();

  s_disp = lv_display_create(BOARD_PANEL_W, BOARD_PANEL_H);
  lv_display_set_default(s_disp);
  lv_display_set_user_data(s_disp, s_panel);
  lv_display_set_flush_cb(s_disp, dispFlush);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
  // DIRECT + double FB + draw_bitmap swap = anti-tear (Espressif/Waveshare)
  lv_display_set_buffers(
      s_disp, s_fb0, s_fb1, (uint32_t)s_fbBytes, LV_DISPLAY_RENDER_MODE_DIRECT);

  s_indev = lv_indev_create();
  lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(s_indev, s_disp);
  lv_indev_set_read_cb(s_indev, touchRead);
  lv_indev_set_scroll_limit(s_indev, 25);
  lv_indev_enable(s_indev, true);

#if LG_THERMA_SIMPLE_UI
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x121214), 0);
  lv_obj_t* label = lv_label_create(scr);
  lv_label_set_text(label, "LG Therma 7B direct");
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(label);
#else
  ESP_LOGI("LVGL", "ui_init...");
  ui_init();
  uiEezInitSignalLeds();
  uiEezNtpLabelInit();
  uiLvglSetPointerInput(true);
#endif

  s_lastTickMs = millis();
  lv_obj_invalidate(lv_screen_active());
  const uint32_t warmStart = millis();
  while (millis() - warmStart < 400) {
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
  }
  touchDrain();
  if (s_indev) {
    lv_indev_reset(s_indev, nullptr);
  }
  s_touchArmed = true;

  s_horRes = BOARD_PANEL_W;
  s_verRes = BOARD_PANEL_H;
  s_initDone = true;
  s_healthGraceUntilMs = millis() + 10000;

  uiNetStartTask();  // Wi-Fi/NTP na druhém jádře — mimo LVGL flush
  ESP_LOGI("LVGL", "init done direct-FB + net task");
}

bool uiLvglInitDone() { return s_initDone; }

void uiLvglRequestLite(bool on) {
  bumpRef(s_liteRef, on);
}

void uiLvglRequestFreeze(bool on) {
  bumpRef(s_freezeRef, on);
}

void uiLvglRequestRecover(bool hard) {
  if (hard) {
    s_reqHardRecover.store(1, std::memory_order_relaxed);
  } else {
    s_reqSoftRecover.store(1, std::memory_order_relaxed);
  }
}

void uiLvglSetFrozen(bool frozen) {
  // UI thread: absolutní — vynuluj/nastav ref a hned aplikuj
  s_freezeRef.store(frozen ? 1 : 0, std::memory_order_relaxed);
  applyDisplayReqs();
}

bool uiLvglIsFrozen() { return s_frozen; }
bool uiLvglIsDisplayLite() { return s_displayLite; }
void uiLvglSetSdioLight(bool on) { (void)on; }

void uiLvglBeginFullPaint(uint32_t holdMs) {
  (void)holdMs;
  if (s_initDone) {
    lv_obj_t* scr = lv_screen_active();
    if (scr) {
      lv_obj_invalidate(scr);
    }
  }
}

bool uiLvglIsFullPaint(void) { return false; }

void uiLvglSetPointerInput(bool enabled) {
  s_pointerInput = enabled;
  if (s_indev) {
    lv_indev_enable(s_indev, enabled);
  }
}

bool uiLvglPointerInputEnabled() { return s_pointerInput; }
uint32_t uiLvglFlushCount() { return s_flushCount; }
uint32_t uiLvglRgbRestartCount() { return s_rgbRestartCount; }

void uiLvglRgbRecover(const char* reason) {
  doRgbRestart(reason);
}

void uiLvglRecoverDisplay(const char* reason) {
  if (!s_initDone || uiDisplayIsAsleep()) {
    return;
  }
  if (s_frozen) {
    s_frozen = false;
    s_frozenSinceMs = 0;
    s_wasFrozen = false;
  }
  resetTouchInput();
  doRgbRestart(reason);
  softRepaint();
  resetTouchInput();
}

void uiLvglScheduleRecover(const char* reason, bool hard, uint32_t delayMs) {
  if (!s_initDone) {
    return;
  }
  const uint32_t due = millis() + delayMs;
  // Soft už čeká → hard zařadit jako follow-up (nesmazat soft)
  if (hard && s_recoverPending && !s_recoverHard) {
    s_recoverFollowHard = true;
    s_recoverFollowDueMs = due;
    s_recoverFollowReason = reason;
    return;
  }
  if (s_recoverPending && s_recoverHard && !hard) {
    return;
  }
  s_recoverPending = true;
  s_recoverHard = hard;
  s_recoverReason = reason;
  s_recoverDueMs = due;
  if (hard) {
    s_recoverFollowHard = false;
  }
}

void uiLvglSetRgbLowBandwidth(bool low) {
  // UI thread: absolutní lite ref
  s_liteRef.store(low ? 1 : 0, std::memory_order_relaxed);
  applyDisplayReqs();
}

int uiLvglHorRes() { return s_horRes; }
int uiLvglVerRes() { return s_verRes; }

void uiLvglTick() {
  if (!s_initDone) {
    return;
  }

  uiDisplayTick();

  const bool asleep = uiDisplayIsAsleep();
  if (s_wasAsleep && !asleep) {
    // Probuzení: soft + hard — DMA často ujede během dlouhého spánku pod Wi-Fi
    uiLvglScheduleRecover("wake", false, 60);
    uiLvglScheduleRecover("wake_hard", true, 380);
  }
  s_wasAsleep = asleep;

  if (asleep) {
    // Displej vypnutý ~90 % času: žádné LVGL / flush / recover
    const uint32_t now = millis();
    if (now != s_lastTickMs) {
      lv_tick_inc(now - s_lastTickMs);
      s_lastTickMs = now;
    }
    return;
  }

  displayHealthWatchdog();
  applyDisplayReqs();
  runScheduledRecover();

  const uint32_t now = millis();
  if (now != s_lastTickMs) {
    lv_tick_inc(now - s_lastTickMs);
    s_lastTickMs = now;
  }

  lv_timer_handler();

#if !LG_THERMA_SIMPLE_UI
  uiNetSyncWifi();
  ui_tick();
  uiEezApplySignalLeds();
  if (!netWifiIsBusy() && !s_displayLite) {
    uiEezNtpLabelTick();
  }
#endif
}
