#include "ui_hmi_canvas.h"

#include <Arduino.h>
#include <M5Unified.h>

#if __has_include(<esp_cache.h>)
#include <esp_cache.h>
#endif

namespace {

constexpr int kFlushLines = 20;

m5gfx::M5Canvas s_canvas;
bool s_ready = false;

void syncCanvasBytes(const void* ptr, size_t bytes) {
#if defined(ESP_CACHE_MSYNC_FLAG_DIR_C2M)
  if (!ptr || bytes == 0) {
    return;
  }
  const uintptr_t start = ((uintptr_t)ptr) & ~127u;
  const uintptr_t end = ((uintptr_t)ptr + bytes + 127u) & ~127u;
  if (start < end) {
    esp_cache_msync((void*)start, end - start,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
  }
#endif
}

}  // namespace

bool uiHmiCanvasInit() {
  if (s_ready) {
    return true;
  }

  M5.Display.setBrightness(48);

  s_canvas.setPsram(true);
  s_canvas.setColorDepth(lgfx::color_depth_t::rgb565_2Byte);
  if (!s_canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    Serial.println("[UI] canvas create failed");
    return false;
  }

  s_ready = true;
  Serial.printf("[UI] canvas %dx%d psram=1\n", s_canvas.width(), s_canvas.height());
  return true;
}

bool uiHmiCanvasReady() {
  return s_ready;
}

lgfx::LovyanGFX& uiHmiCanvas() {
  return s_canvas;
}

int uiHmiCanvasFlushBandHeight() {
  return kFlushLines;
}

bool uiHmiCanvasFlushBandAt(int y) {
  if (!s_ready) {
    return false;
  }

  const int w = s_canvas.width();
  const int h = s_canvas.height();
  if (y < 0 || y >= h) {
    return false;
  }

  int bandH = kFlushLines;
  if (y + bandH > h) {
    bandH = h - y;
  }

  const auto* buf = static_cast<const uint16_t*>(s_canvas.getBuffer());
  if (!buf) {
    s_canvas.pushSprite(&M5.Display, 0, 0);
    M5.Display.waitDMA();
    return false;
  }

  const size_t byteLen = (size_t)w * (size_t)bandH * sizeof(uint16_t);
  syncCanvasBytes(buf + ((size_t)y * (size_t)w), byteLen);
  M5.Display.startWrite();
  M5.Display.pushImage(0, y, w, bandH, buf + ((size_t)y * (size_t)w));
  M5.Display.endWrite();

  return (y + bandH) < h;
}
