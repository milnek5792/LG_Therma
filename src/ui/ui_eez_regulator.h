#ifndef UI_EEZ_REGULATOR_H
#define UI_EEZ_REGULATOR_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* lbl_title;
  lv_obj_t* btn_mode;
  lv_obj_t* btn_save;
  lv_obj_t* btn_ekv;
  lv_obj_t* chart;
  lv_chart_series_t* ser_room;
  lv_chart_series_t* ser_sp;
  lv_chart_series_t* ser_u;
  lv_obj_t* lbl_live;
  lv_obj_t* lbl_curve;
  lv_obj_t* lbl_pid;
  lv_obj_t* btn_kp_m;
  lv_obj_t* btn_kp_p;
  lv_obj_t* btn_ki_m;
  lv_obj_t* btn_ki_p;
  lv_obj_t* btn_kd_m;
  lv_obj_t* btn_kd_p;
  lv_obj_t* btn_bias_m;
  lv_obj_t* btn_bias_p;
  lv_obj_t* btn_cold_m;
  lv_obj_t* btn_cold_p;
  lv_obj_t* btn_warm_m;
  lv_obj_t* btn_warm_p;
} regulator_objects_t;

extern regulator_objects_t regulatorObj;

void uiRegulatorCreate(void);
void uiRegulatorEnsureCreated(void);
void uiRegulatorOnLeave(void);
void uiRegulatorTick(void);
lv_obj_t* uiRegulatorScreen(void);

#ifdef __cplusplus
}
#endif

#endif
