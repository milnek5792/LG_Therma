#ifndef UI_EEZ_PLAN_H
#define UI_EEZ_PLAN_H

#include "lg_lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t* screen;
  lv_obj_t* btn_back;
  lv_obj_t* btn_toggle;
  lv_obj_t* lbl_title;
  lv_obj_t* lbl_col_den;
  lv_obj_t* lbl_col_obdobi[5];
  lv_obj_t* lbl_casy[5];
  lv_obj_t* btn_cas_zac[5];
  lv_obj_t* btn_cas_kon[5];
  lv_obj_t* lbl_dny[7];
  lv_obj_t* btn_bunky[7][5];
} plan_objects_t;

extern plan_objects_t planObj;

void uiPlanCreate(void);
void uiPlanTick(void);
void uiPlanRefreshAll(void);
lv_obj_t* uiPlanScreen(void);

#ifdef __cplusplus
}
#endif

#endif
