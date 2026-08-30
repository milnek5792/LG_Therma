// bus_lg_config.h — sjednocená konfigurace (Tab5 + 7B)
#ifndef LG_CONFIG_H
#define LG_CONFIG_H

#include "lg_board.h"

// Struktura sketchu:
//   koren/     LG_Therma.ino, lv_conf.h, build_opt.h, bus_task_*, ui_task_*
//   include/   lg_board.h, app_cmd.h, climate_*, board_7b.h …
//   src/       bus, ui, climate, net, app …
//   ui_eez/    EEZ export + skripty

#ifndef LG_USE_EEZ_LVGL
#define LG_USE_EEZ_LVGL 1
#endif

// --- LIN UART piny ---
#if LG_BOARD_7B
  #ifndef LG_MBUS_RX_PIN
  #define LG_MBUS_RX_PIN 44
  #endif
  #ifndef LG_MBUS_TX_PIN
  #define LG_MBUS_TX_PIN 43
  #endif
  #ifndef LG_UART_NUM
  #define LG_UART_NUM 2
  #endif
#else
  #ifndef LG_MBUS_RX_PIN
  #define LG_MBUS_RX_PIN 38
  #endif
  #ifndef LG_MBUS_TX_PIN
  #define LG_MBUS_TX_PIN 37
  #endif
  #ifndef LG_UART_NUM
  #define LG_UART_NUM 1
  #endif
#endif

#ifndef LG_BAUDRATE
#define LG_BAUDRATE 300
#endif

// Legacy aliasy (bus_lg_lin.h)
#define TAB5_MBUS_RX_PIN LG_MBUS_RX_PIN
#define TAB5_MBUS_TX_PIN LG_MBUS_TX_PIN

/** 1 = lgBusTick() v dedikovaném lin tasku (7B architektura). 0 = v loop/task lg_bus. */
#ifndef LG_LIN_DEDICATED_TASK
#define LG_LIN_DEDICATED_TASK 1
#endif

#ifndef LG_LIN_IN_LOOP
#define LG_LIN_IN_LOOP (!LG_LIN_DEDICATED_TASK)
#endif

#ifndef LG_DEFER_LIN_START
#define LG_DEFER_LIN_START 1
#endif

#ifndef LG_LIN_START_DELAY_MS
#if LG_BOARD_7B
#define LG_LIN_START_DELAY_MS 1500
#else
#define LG_LIN_START_DELAY_MS 2000
#endif
#endif

#ifndef LG_LIN_TASK_PRIO
#define LG_LIN_TASK_PRIO 12
#endif

#ifndef LG_LIN_QUIET_PARSE
#define LG_LIN_QUIET_PARSE LG_BOARD_7B
#endif

#ifndef LG_A0_FRESH_MS
#define LG_A0_FRESH_MS 60000u
#endif

#ifndef LG_A0_SLOW_PERIOD_MS
#define LG_A0_SLOW_PERIOD_MS 25000u
#endif

#ifndef UI_SP_PENDING_MARGIN_MS
#define UI_SP_PENDING_MARGIN_MS 5000u
#endif

#ifndef LG_KRATKY_LOG
#define LG_KRATKY_LOG LG_BOARD_7B
#endif

// --- FreeRTOS jádra ---
#define LG_CORE_LIN 1
#define LG_CORE_UI  1
#define LG_CORE_NET 0

#define LG_TASK_LIN_STACK 12288
#define LG_TASK_UI_STACK  65536
#define LG_TASK_NET_STACK 28672

#define LG_TASK_LIN_PRIO  LG_LIN_TASK_PRIO
#define LG_TASK_UI_PRIO   2
#define LG_TASK_NET_PRIO  1

#ifndef LG_DISPLAY_SLEEP_MINUTES
#define LG_DISPLAY_SLEEP_MINUTES 10
#endif

#ifndef CLIMATE_TICHY_NOC_START_H
#define CLIMATE_TICHY_NOC_START_H 22
#endif

#ifndef CLIMATE_TICHY_NOC_END_H
#define CLIMATE_TICHY_NOC_END_H 6
#endif

#ifndef CLIMATE_TICHY_AUTO_NOC
#define CLIMATE_TICHY_AUTO_NOC 1
#endif

#endif
