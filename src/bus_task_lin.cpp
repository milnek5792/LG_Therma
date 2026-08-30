#include "bus_task_lin.h"
#include "bus_lg_config.h"
#include "src/bus_lg_lin_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>

static bool s_linStarted = false;

#if !LG_LIN_IN_LOOP

static TaskHandle_t s_linTask = nullptr;

static void lgTaskLin(void* param) {
  (void)param;
  lgBusInit();
  Serial.printf("[LIN] task bezi na core %d\n", xPortGetCoreID());

  for (;;) {
    lgBusTick();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

#endif

void lgTaskLinStart() {
  if (s_linStarted) { return; }

#if LG_LIN_IN_LOOP
  s_linStarted = true;
  Serial.println("[LIN] odposlech v loop() — UART jiz init v setup()");
#else
  BaseType_t ok = xTaskCreatePinnedToCore(
      lgTaskLin, "lg_bus", LG_TASK_LIN_STACK, nullptr, LG_TASK_LIN_PRIO,
      &s_linTask, LG_CORE_LIN);
  if (ok != pdPASS) {
    Serial.println("[LIN] CHYBA: nelze vytvorit task (nedostatek pameti?)");
    return;
  }

  s_linStarted = true;
  Serial.printf("[LIN] task vytvoren (core %d, stack %u B)\n", LG_CORE_LIN,
                (unsigned)LG_TASK_LIN_STACK);
#endif
}

bool lgTaskLinIsRunning() {
  return s_linStarted;
}
