#ifndef UI_HMI_LAYOUT_H
#define UI_HMI_LAYOUT_H

#include "ui_eez_model.h"
#include "bus_lg_model.h"

#define BTN_TICHY_X   1040
#define BTN_TICHY_Y   12
#define BTN_TICHY_W   210
#define BTN_TICHY_H   36

#define BTN_MINUS_X   107
#define BTN_MINUS_Y   151
#define BTN_MINUS_W   100
#define BTN_MINUS_H   100

#define BTN_PLUS_X    488
#define BTN_PLUS_Y    152
#define BTN_PLUS_W    100
#define BTN_PLUS_H    100

#define BTN_RUN_X     865
#define BTN_RUN_Y     90
#define BTN_RUN_W     195
#define BTN_RUN_H     60

#define BTN_STOP_X    1070
#define BTN_STOP_Y    90
#define BTN_STOP_W    195
#define BTN_STOP_H    60

#define BTN_MENU_X    1024
#define BTN_MENU_Y    600
#define BTN_MENU_W    256
#define BTN_MENU_H    120

enum HmiDotykBtn : int8_t {
  HMI_DOTYK_ZADNY = -1,
  HMI_DOTYK_MINUS,
  HMI_DOTYK_PLUS,
  HMI_DOTYK_RUN,
  HMI_DOTYK_STOP,
  HMI_DOTYK_TICHY,
  HMI_DOTYK_MENU,
};

inline bool hmiInRect(int tx, int ty, int x, int y, int w, int h, int pad = 12) {
  return tx >= (x - pad) && tx < (x + w + pad) && ty >= (y - pad) && ty < (y + h + pad);
}

inline HmiDotykBtn hmiZjistiTlacitko(int tx, int ty) {
  if (hmiInRect(tx, ty, BTN_TICHY_X, BTN_TICHY_Y, BTN_TICHY_W, BTN_TICHY_H)) {
    return HMI_DOTYK_TICHY;
  }
  if (hmiInRect(tx, ty, BTN_MINUS_X, BTN_MINUS_Y, BTN_MINUS_W, BTN_MINUS_H)) {
    return HMI_DOTYK_MINUS;
  }
  if (hmiInRect(tx, ty, BTN_PLUS_X, BTN_PLUS_Y, BTN_PLUS_W, BTN_PLUS_H)) {
    return HMI_DOTYK_PLUS;
  }
  if (hmiInRect(tx, ty, BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H)) {
    return HMI_DOTYK_RUN;
  }
  if (hmiInRect(tx, ty, BTN_STOP_X, BTN_STOP_Y, BTN_STOP_W, BTN_STOP_H)) {
    return HMI_DOTYK_STOP;
  }
  if (hmiInRect(tx, ty, BTN_MENU_X, BTN_MENU_Y, BTN_MENU_W, BTN_MENU_H)) {
    return HMI_DOTYK_MENU;
  }
  return HMI_DOTYK_ZADNY;
}

inline void hmiProvedAkci(HmiDotykBtn btn) {
  switch (btn) {
    case HMI_DOTYK_MINUS:
      uiEez.akce_tlacitko = UI_AKCE_TEPLOTA_MINUS;
      break;
    case HMI_DOTYK_PLUS:
      uiEez.akce_tlacitko = UI_AKCE_TEPLOTA_PLUS;
      break;
    case HMI_DOTYK_RUN:
    case HMI_DOTYK_STOP:
      uiEez.akce_tlacitko = UI_AKCE_START_STOP;
      break;
    case HMI_DOTYK_TICHY:
      uiEez.sig_utlum = !uiEez.sig_utlum;
      potrebaObnovitDisplej = true;
      break;
    case HMI_DOTYK_MENU:
      break;
    default:
      break;
  }
}

inline const char* hmiBtnName(HmiDotykBtn btn) {
  switch (btn) {
    case HMI_DOTYK_MINUS: return "minus";
    case HMI_DOTYK_PLUS: return "plus";
    case HMI_DOTYK_RUN: return "run";
    case HMI_DOTYK_STOP: return "stop";
    case HMI_DOTYK_TICHY: return "tichy";
    case HMI_DOTYK_MENU: return "menu";
    default: return "?";
  }
}

#endif
