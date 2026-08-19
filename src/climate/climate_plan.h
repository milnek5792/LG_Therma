#ifndef CLIMATE_PLAN_H
#define CLIMATE_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLAN_POCET_OBDOBI 5
#define PLAN_POCET_DNU 7

/** VT1–VT4 = vysoký tarif, NOC = noční útlum. */
enum PlanTypObdobi : uint8_t {
  PLAN_OBDOBI_VT1 = 0,
  PLAN_OBDOBI_VT2,
  PLAN_OBDOBI_VT3,
  PLAN_OBDOBI_VT4,
  PLAN_OBDOBI_NOC,
};

enum PlanAkce : uint8_t {
  PLAN_AKCE_NORMAL = 0,
  PLAN_AKCE_UTLUM = 1,
  PLAN_AKCE_VYP = 2,
};

struct PlanCas {
  uint8_t hodina;
  uint8_t minuta;
};

struct PlanObdobiCas {
  PlanCas zacatek;
  PlanCas konec;
};

struct PlanBunka {
  PlanAkce akce;
  uint8_t utlum_stupne;
};

struct PlanTydenConfig {
  bool aktivni;
  PlanObdobiCas obdobi[PLAN_POCET_OBDOBI];
  PlanBunka tabulka[PLAN_POCET_DNU][PLAN_POCET_OBDOBI];
};

extern PlanTydenConfig g_planConfig;

void climatePlanInit(void);
void climatePlanTick(void);
void climatePlanSave(void);
void climatePlanSetDefaults(void);

const PlanTydenConfig* climatePlanGetConfig(void);
PlanTydenConfig* climatePlanGetConfigMutable(void);

/** Aktuálně aktivní období (-1 = mimo plán / normální režim). */
int climatePlanAktivniObdobi(void);
bool climatePlanJeAktivni(void);
bool climatePlanAplikujeUtlum(void);

const char* climatePlanDenNazev(uint8_t denIndex);
const char* climatePlanObdobiNazev(uint8_t obdobiIndex);
const char* climatePlanAkceText(PlanAkce akce, uint8_t utlumStupne);

#ifdef __cplusplus
}
#endif

#endif
