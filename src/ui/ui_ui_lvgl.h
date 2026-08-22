// ui_ui_lvgl.h — LVGL + EEZ HMI (Waveshare 7B)
#ifndef UI_LVGL_H
#define UI_LVGL_H

#include "lg_lvgl.h"

void uiLvglInit();
void uiLvglTick();

void uiLvglSetFrozen(bool frozen);
bool uiLvglIsFrozen();
bool uiLvglIsDisplayLite();

void uiLvglSetSdioLight(bool on);
void uiLvglBeginFullPaint(uint32_t holdMs);
bool uiLvglIsFullPaint(void);

void uiLvglSetPointerInput(bool enabled);
bool uiLvglPointerInputEnabled();

bool uiLvglInitDone();
uint32_t uiLvglFlushCount();
uint32_t uiLvglRgbRestartCount();
/** Po Wi-Fi/MQTT spike: znovu seřadí RGB DMA (anti permanent shift). */
void uiLvglRgbRecover(const char* reason);
/** Soft: full redraw. Hard: RGB restart + soft. Okamžité. */
void uiLvglRecoverDisplay(const char* reason);
/** Odložená obnova — neblokuje event handler (preferovat před RecoverDisplay). */
void uiLvglScheduleRecover(const char* reason, bool hard, uint32_t delayMs);
/** No-op: PCLK je pevný (runtime změna bliká celou plochu). */
void uiLvglSetRgbLowBandwidth(bool low);
int uiLvglHorRes();
int uiLvglVerRes();

#endif
