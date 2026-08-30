// ui_lvgl_port.cpp — LVGL 9 display + touch on M5Stack Tab5 (M5Unified)
#include "ui_ui_lvgl.h"
#include "ui_eez_ui.h"
#include "ui_eez_screens.h"
#include "ui_eez_vars.h"
#include "ui_display_bus.h"
#include "ui_touch_tab5.h"
#include "ui_eez_signal_leds.h"
#include "ui_eez_ntp_label.h"
#include "ui_display_sleep.h"
#include "net_sdio_arbiter.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

static constexpr int kHorRes = 1280;
// Menší partial buffer v INTERNAL — EXT_RAM_BSS + Wi‑Fi/SDIO = Store fault 0x500d2000
static constexpr int kDrawLines = 16;

static lv_display_t* s_disp = nullptr;
static lv_indev_t* s_indev = nullptr;
static lv_color_t* s_drawBuf = nullptr;
static size_t s_drawBufBytes = 0;
static uint32_t s_lastTickMs = 0;
static uint32_t s_flushCount = 0;
static volatile bool s_initDone = false;
static volatile bool s_frozen = false;
static volatile bool s_sdioLight = false;
static volatile bool s_fullPaint = false;
static volatile bool s_unfreezeRefresh = false;
static uint32_t s_fullPaintUntilMs = 0;
static uint32_t s_lastFlushMs = 0;
static bool s_pointerInput = false;
static int s_horRes = 0;
static int s_verRes = 0;

static bool fullPaintActive() {
  if (!s_fullPaint) { return false; }
  if ((int32_t)(s_fullPaintUntilMs - millis()) <= 0) {
    s_fullPaint = false;
    return false;
  }
  return true;
}

static void uiApplyTichyRezimVisual() {
  if (!objects.btn_tichy) { return; }
  static bool initialized = false;
  static bool lastUtlum = false;
  const bool utlum = get_var_sig_utlum();
  if (initialized && utlum == lastUtlum) { return; }
  initialized = true;
  lastUtlum = utlum;
  lv_obj_set_style_bg_color(
      objects.btn_tichy,
      lv_color_hex(utlum ? 0xffaa00u : 0x48484fu),
      LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_invalidate(objects.btn_tichy);
  uiDisplayNoteActivity();
}

static void lvglPump(uint32_t ms) {
  lv_tick_inc(ms);
  lv_timer_handler();
}

static void dispFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  // Freeze: nic na panel — ALE nesmíme zahodit dirty bez invalidate po unfreeze
  // (viz uiLvglSetFrozen). Během freeze nevolat push*.
  if (s_frozen) {
    lv_display_flush_ready(disp);
    return;
  }
  // Tab5: Wi‑Fi/MQTT jde přes SDIO a žere MALLOC_CAP_DMA.
  // pushImageDMA s tím soupeří → dma_max klesne na ~2 KB a startTLS assertne
  // (sdio_rx_get_buffer). Proto vždy CPU blit — nikdy display DMA.
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (uiDisplayBusLock(portMAX_DELAY)) {
    M5.Display.pushImage(
        area->x1, area->y1, w, h, reinterpret_cast<const uint16_t*>(pxMap));
    uiDisplayBusUnlock();
  }
  s_lastFlushMs = millis();
  ++s_flushCount;
  lv_display_flush_ready(disp);
}

static int16_t s_touchLastX = 0;
static int16_t s_touchLastY = 0;

static void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  if (s_pointerInput) {
    M5.update();
  }
  if (M5.Touch.getCount() > 0) {
    auto detail = M5.Touch.getDetail(0);
    if (detail.isPressed()) {
      s_touchLastX = detail.x;
      s_touchLastY = detail.y;
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = s_touchLastX;
      data->point.y = s_touchLastY;
      uiDisplayNoteActivity();
      return;
    }
  }
  data->state = LV_INDEV_STATE_RELEASED;
  data->point.x = s_touchLastX;
  data->point.y = s_touchLastY;
}

void uiLvglInit() {
  uiDisplayBusInit();
  const int horRes = M5.Display.width();
  const int verRes = M5.Display.height();

  s_drawBufBytes = (size_t)kHorRes * (size_t)kDrawLines * sizeof(lv_color_t);
  // Prefer INTERNAL (ne PSRAM) — Tab5 Wi‑Fi/SDIO + PSRAM draw = Core 1 Store fault
  s_drawBuf = (lv_color_t*)heap_caps_malloc(
      s_drawBufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!s_drawBuf) {
    s_drawBuf = (lv_color_t*)heap_caps_malloc(
        s_drawBufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!s_drawBuf) {
    Serial.println("[LVGL] draw buf ALLOC FAIL");
    return;
  }
  Serial.printf("[LVGL] draw buf %u KB @ %p (%s)\n",
                (unsigned)(s_drawBufBytes / 1024), (void*)s_drawBuf,
                ((uintptr_t)s_drawBuf >= 0x48000000u) ? "PSRAM?" : "INTERNAL");

  lv_init();

  s_disp = lv_display_create(horRes, verRes);
  lv_display_set_default(s_disp);
  lv_display_set_flush_cb(s_disp, dispFlush);
  lv_display_set_buffers(
      s_disp, s_drawBuf, nullptr, (uint32_t)s_drawBufBytes,
      LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

  s_indev = lv_indev_create();
  lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(s_indev, s_disp);
  lv_indev_set_read_cb(s_indev, touchRead);
  lv_indev_enable(s_indev, false);

  // NVS jas/usínání PŘED vytvořením obrazovek — jinak settings zůstane na default 60%/2min.
  uiDisplayInit();
  ui_init();
  uiEezInitSignalLeds();
  uiEezNtpLabelInit();
  uiTouchVisualInit();
  s_lastTickMs = millis();

  lv_obj_invalidate(lv_screen_active());
  lvglPump(1);
  lv_refr_now(s_disp);
  for (int i = 0; i < 8; ++i) {
    lvglPump(5);
  }

  s_horRes = horRes;
  s_verRes = verRes;

  s_initDone = true;
  Serial.println("[LVGL] init done");
}

bool uiLvglInitDone() { return s_initDone; }

void uiLvglSetFrozen(bool frozen) {
  if (s_frozen == frozen) { return; }
  s_frozen = frozen;
  Serial.printf("[LVGL] %s\n", frozen ? "freeze" : "unfreeze");
  if (!frozen && s_initDone) {
    // Po MQTT: bez full-screen invalidate — jinak SDIO shodí session (state=-3).
    if (!netSdioMqttSession()) {
      s_unfreezeRefresh = true;
      s_lastFlushMs = 0;
    }
  }
}

bool uiLvglIsFrozen() { return s_frozen; }

void uiLvglSetSdioLight(bool on) {
  if (s_sdioLight == on) { return; }
  s_sdioLight = on;
  // Jen při skutečné změně (ne spam při MQTT poll)
  Serial.printf("[LVGL] sdio-light %s\n", on ? "ON" : "OFF");
}

void uiLvglBeginFullPaint(uint32_t holdMs) {
  s_fullPaint = true;
  s_fullPaintUntilMs = millis() + holdMs;
  s_lastFlushMs = 0;
  Serial.printf("[LVGL] full-paint %lu ms\n", (unsigned long)holdMs);
}

bool uiLvglIsFullPaint(void) {
  return fullPaintActive();
}

void uiLvglSetPointerInput(bool enabled) {
  s_pointerInput = enabled;
  if (s_indev) {
    lv_indev_enable(s_indev, enabled);
  }
}
bool uiLvglPointerInputEnabled() { return s_pointerInput; }
uint32_t uiLvglFlushCount() { return s_flushCount; }
int uiLvglHorRes() { return s_horRes; }
int uiLvglVerRes() { return s_verRes; }

void uiLvglTick() {
  // TLS/MQTT: ani tick — žádný invalidate/flush na Core 1
  if (netSdioTlsBusy()) {
    return;
  }
  if (s_frozen) {
    const uint32_t now = millis();
    if (now != s_lastTickMs) {
      lv_tick_inc(now - s_lastTickMs);
      s_lastTickMs = now;
    }
    return;
  }
  const uint32_t now = millis();
  const uint32_t elapsed = now - s_lastTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    s_lastTickMs = now;
  }
  // Po TLS/BLE freeze: nejdřív model→widgety, teprve pak flush
  if (s_unfreezeRefresh) {
    s_unfreezeRefresh = false;
    ui_tick();
    uiEezApplySignalLeds();
    uiEezNtpLabelTick();
    uiApplyTichyRezimVisual();
    lv_obj_t* scr = lv_screen_active();
    if (scr) { lv_obj_invalidate(scr); }
  }
  uiTouchVisualSync();
  lv_timer_handler();
  ui_tick();
  uiEezApplySignalLeds();
  uiEezNtpLabelTick();
  uiApplyTichyRezimVisual();
}
