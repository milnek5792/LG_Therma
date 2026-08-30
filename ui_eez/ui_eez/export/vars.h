#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_NONE
};
extern float get_var_teplota_vody_set();
extern void set_var_teplota_vody_set(float value);
extern float get_var_teplota_vody_vstup();
extern void set_var_teplota_vody_vstup(float value);
extern float get_var_teplota_vody_vystup();
extern void set_var_teplota_vody_vystup(float value);
extern float get_var_teplota_vnitrni();
extern void set_var_teplota_vnitrni(float value);
extern float get_var_teplota_venkovni();
extern void set_var_teplota_venkovni(float value);
extern float get_var_teplota_spad();
extern void set_var_teplota_spad(float value);
extern int32_t get_var_rezim();
extern void set_var_rezim(int32_t value);
extern int32_t get_var_stav_tc();
extern void set_var_stav_tc(int32_t value);
extern bool get_var_sig_chod();
extern void set_var_sig_chod(bool value);
extern bool get_var_sig_cerpadlo();
extern void set_var_sig_cerpadlo(bool value);
extern bool get_var_sig_kompresor();
extern void set_var_sig_kompresor(bool value);
extern bool get_var_sig_el_topeni();
extern void set_var_sig_el_topeni(bool value);
extern bool get_var_sig_odmrazovani();
extern void set_var_sig_odmrazovani(bool value);
extern bool get_var_sig_wifi();
extern void set_var_sig_wifi(bool value);
extern bool get_var_sig_mqtt();
extern void set_var_sig_mqtt(bool value);
extern bool get_var_sig_ble();
extern void set_var_sig_ble(bool value);
extern bool get_var_sig_utlum();
extern void set_var_sig_utlum(bool value);
extern bool get_var_sig_alarm();
extern void set_var_sig_alarm(bool value);
extern const char *get_var_cas_text();
extern void set_var_cas_text(const char *value);
extern const char *get_var_datum_text();
extern void set_var_datum_text(const char *value);
extern bool get_var_cas_platny();
extern void set_var_cas_platny(bool value);
extern int32_t get_var_akce_tlacitko();
extern void set_var_akce_tlacitko(int32_t value);
extern const char *get_var_plan_text();
extern void set_var_plan_text(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/