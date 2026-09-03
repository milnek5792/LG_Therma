#ifndef UI_EEZ_ENERGY_H
#define UI_EEZ_ENERGY_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_summary;
  lv_obj_t* btn_day_prev;
  lv_obj_t* btn_day_next;
  lv_obj_t* lbl_day;
  lv_obj_t* lbl_day_kwh;
  lv_obj_t* chart_power;
  lv_chart_series_t* ser_power;
  lv_obj_t* band_layer;
  lv_obj_t* chart_month;
  lv_chart_series_t* ser_month;
  lv_obj_t* month_val_layer;
  lv_obj_t* lbl_years;
  lv_obj_t* lbl_y_unit;
} energy_objects_t;

extern energy_objects_t energyObj;

void uiEnergyCreate(void);
void uiEnergyEnsureCreated(void);
void uiEnergyTick(void);
lv_obj_t* uiEnergyScreen(void);

#ifdef __cplusplus
}
#endif

#endif
