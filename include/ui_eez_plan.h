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
  lv_obj_t* btn_cas_karta[5];
  lv_obj_t* lbl_dny[7];
  lv_obj_t* btn_bunky[7][5];
  lv_obj_t* modal_bg;
  lv_obj_t* modal_panel;
  lv_obj_t* modal_title;
  lv_obj_t* modal_lbl_od;
  lv_obj_t* modal_btn_od_minus;
  lv_obj_t* modal_btn_od_plus;
  lv_obj_t* modal_lbl_delka;
  lv_obj_t* modal_btn_del_minus;
  lv_obj_t* modal_btn_del_plus;
  lv_obj_t* modal_btn_hotovo;
} plan_objects_t;

extern plan_objects_t planObj;

void uiPlanCreate(void);
void uiPlanEnsureCreated(void);
void uiPlanOnLeave(void);
void uiPlanMarkDirty(void);
void uiPlanFlushSave(void);
void uiPlanTick(void);
void uiPlanRefreshAll(void);
lv_obj_t* uiPlanScreen(void);

#ifdef __cplusplus
}
#endif

#endif
