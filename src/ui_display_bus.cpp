#include "ui_display_bus.h"

#include "freertos/semphr.h"

static SemaphoreHandle_t s_displayMu;

void uiDisplayBusInit() {
  if (!s_displayMu) {
    s_displayMu = xSemaphoreCreateMutex();
  }
}

bool uiDisplayBusLock(TickType_t timeoutTicks) {
  if (!s_displayMu) { return true; }
  return xSemaphoreTake(s_displayMu, timeoutTicks) == pdTRUE;
}

void uiDisplayBusUnlock() {
  if (s_displayMu) {
    xSemaphoreGive(s_displayMu);
  }
}
