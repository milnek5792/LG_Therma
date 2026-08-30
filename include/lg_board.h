// lg_board.h — výběr desky (Tab5 + H2 UART / Waveshare 7B + NimBLE)
#ifndef LG_BOARD_H
#define LG_BOARD_H

/**
 * Board profil — nastav v build flags:
 *   Tab5 (default Arduino):  -DLG_BOARD_TAB5=1
 *   Waveshare 7B (PIO):      -DLG_BOARD_7B=1
 */
#ifndef LG_BOARD_TAB5
#define LG_BOARD_TAB5 0
#endif
#ifndef LG_BOARD_7B
#define LG_BOARD_7B 0
#endif

#if LG_BOARD_7B
  #undef LG_BOARD_TAB5
  #define LG_BOARD_TAB5 0
#elif !LG_BOARD_TAB5
  #define LG_BOARD_TAB5 1
#endif

#if LG_BOARD_TAB5 && LG_BOARD_7B
  #error "Pouze jeden board profil: LG_BOARD_TAB5 nebo LG_BOARD_7B"
#endif

/** Tab5: M5Unified, SDIO arbiter, touch v loop(), H2 UART pokojová T */
#define LG_HAS_M5UNIFIED        LG_BOARD_TAB5
/** 7B: Waveshare RGB + GT911 */
#define LG_HAS_WAVESHARE_RGB    LG_BOARD_7B
/** NimBLE na hlavní desce (7B S3) */
#define LG_HAS_NATIVE_BLE       LG_BOARD_7B
/** H2 bridge přes UART G6/G7 (Tab5) */
#define LG_HAS_H2_UART_ROOM     LG_BOARD_TAB5
/** Tab5: samostatný LVGL task kvůli SDIO/TLS */
#define LG_UI_IN_DEDICATED_TASK LG_BOARD_TAB5
/** Tab5: net_sdio_arbiter pro TLS/UI */
#define LG_HAS_SDIO_ARBITER     LG_BOARD_TAB5

/** Rozměry panelu pro nativní obrazovky (plan/regulator) */
#if LG_BOARD_7B
#include "board_7b.h"
#else
#define BOARD_PANEL_W 1280
#define BOARD_PANEL_H 720
#define BOARD_DESIGN_W 1280
#define BOARD_DESIGN_H 720
#endif

#endif
