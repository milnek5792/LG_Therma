// ui_lvgl.h — LVGL + EEZ HMI (Tab5)
#ifndef UI_LVGL_H
#define UI_LVGL_H

#include "lg_lvgl.h"

void uiLvglInit();
void uiLvglTick();

/** Během MQTT TLS — neflushovat (šetří SDIO/DMA pro Wi-Fi). */
void uiLvglSetFrozen(bool frozen);
bool uiLvglIsFrozen();

/** Při MQTT online — max. ~5 flush/s (SDIO sdílené s Wi-Fi). */
void uiLvglSetSdioLight(bool on);

/**
 * Přepnutí obrazovky: plný CPU paint bez skip/throttle.
 * (sdio-light jinak „rozbije“ novou obrazovku)
 */
void uiLvglBeginFullPaint(uint32_t holdMs);
bool uiLvglIsFullPaint(void);

void uiLvglSetPointerInput(bool enabled);
bool uiLvglPointerInputEnabled();

// Stav po init (cteni z loop/core 1 — Serial z UI tasku na Tab5 casto nefunguje).
bool uiLvglInitDone();
uint32_t uiLvglFlushCount();
int uiLvglHorRes();
int uiLvglVerRes();

#endif
