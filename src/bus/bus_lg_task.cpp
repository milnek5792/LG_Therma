// bus_lg_task.cpp — lgBusTick ve vlastní úloze (core 1, prio LG_LIN_TASK_PRIO)
#include "bus_lg_task.h"

#include "bus_lg_config.h"
#include "bus_lg_lin_api.h"

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

static const char* TAG = "LIN_TASK";
TaskHandle_t s_linTask = nullptr;
volatile bool s_running = false;

void linTask(void* /*arg*/) {
#if LG_DEFER_LIN_START
  if (LG_LIN_START_DELAY_MS > 0) {
    vTaskDelay(pdMS_TO_TICKS(LG_LIN_START_DELAY_MS));
    ESP_LOGI(TAG, "deferred start %u ms", (unsigned)LG_LIN_START_DELAY_MS);
  }
#endif

  lgBusInit();
  s_running = true;
  ESP_LOGI(TAG, "task run core=%d prio=%u RX=%d TX=%d",
           (int)xPortGetCoreID(),
           (unsigned)uxTaskPriorityGet(nullptr),
           LG_MBUS_RX_PIN,
           LG_MBUS_TX_PIN);

  for (;;) {
    lgBusTick();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void lgBusStartTask(void) {
  if (s_linTask) {
    return;
  }
  constexpr UBaseType_t kLinPrio = (UBaseType_t)LG_LIN_TASK_PRIO;
  constexpr uint32_t kLinStack = 12288;
  BaseType_t ok = xTaskCreatePinnedToCore(linTask, "lin", kLinStack, nullptr,
                                          kLinPrio, &s_linTask, 1);
  ESP_LOGI(TAG, "xTaskCreatePinnedToCore -> %d handle=%p stack=%u prio=%u",
           (int)ok, (void*)s_linTask, (unsigned)kLinStack, (unsigned)kLinPrio);
}

bool lgBusTaskRunning(void) {
  return s_running;
}
