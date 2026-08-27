// ui_ui_lvgl.h — LVGL + EEZ HMI (Waveshare 7B)
#ifndef UI_LVGL_H
#define UI_LVGL_H

#include "lg_lvgl.h"

void uiLvglInit();
void uiLvglTick();

/**
 * Display owner = jen uiLvglTick (loop / core 1).
 * Ostatní jádra/tasky smí jen Request* — aplikace v ticku.
 */

/** Lite režim (BLE scan) — refcount, thread-safe. */
void uiLvglRequestLite(bool on);
/** Freeze LVGL (TLS / Wi‑Fi spike) — refcount, thread-safe. */
void uiLvglRequestFreeze(bool on);
/** Jednorázová obnova RGB (soft/hard) — thread-safe. */
void uiLvglRequestRecover(bool hard);

/** Absolutní stav (UI thread). Preferovat Request* z net/MQTT/BLE. */
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
/**
 * Přímý lite (jen UI thread). Z net/BLE použij uiLvglRequestLite.
 * Při lite→off spustí RGB recover.
 */
void uiLvglSetRgbLowBandwidth(bool low);
int uiLvglHorRes();
int uiLvglVerRes();

#endif
