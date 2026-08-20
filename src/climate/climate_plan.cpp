#include "climate_plan.h"

#include "storage_config_nvs.h"
#include "ui_bus_bindings.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

PlanTydenConfig g_planConfig;

namespace {

const char* kDny[] = {"Po", "Ut", "St", "Ct", "Pa", "So", "Ne"};
const char* kObdobi[] = {"VT1", "VT2", "VT3", "VT4", "Noc"};

int s_aplikovaneObdobi = -1;
PlanAkce s_aplikovanaAkce = PLAN_AKCE_NORMAL;
uint8_t s_aplikovanyUtlum = 0;
uint8_t s_zakladniTeplota = 40;
bool s_zakladniZapnuto = false;
bool s_planOvlada = false;
uint32_t s_lastMonitorMs = 0;

uint16_t casNaMinuty(const PlanCas* cas) {
  return static_cast<uint16_t>(cas->hodina) * 60u + cas->minuta;
}

bool casVRozsahu(uint16_t nowMin, uint16_t startMin, uint16_t endMin) {
  if (startMin == endMin) {
    return false;
  }
  if (startMin < endMin) {
    return nowMin >= startMin && nowMin < endMin;
  }
  return nowMin >= startMin || nowMin < endMin;
}

void nastavObdobi(PlanObdobiCas* obdobi, uint8_t zH, uint8_t zM, uint8_t kH, uint8_t kM,
                  PlanCasRezim rezim = PLAN_CAS_OD_DO) {
  obdobi->zacatek.hodina = zH;
  obdobi->zacatek.minuta = zM;
  obdobi->konec.hodina = kH;
  obdobi->konec.minuta = kM;
  obdobi->cas_rezim = rezim;
}

void nastavBunku(PlanBunka* bunka, PlanAkce akce, uint8_t stupne) {
  bunka->akce = akce;
  bunka->utlum_stupne = stupne;
}

int najdiAktivniObdobi(uint16_t nowMin) {
  for (int i = 0; i < PLAN_POCET_OBDOBI; ++i) {
    const PlanObdobiCas* ob = &g_planConfig.obdobi[i];
    const uint16_t zac = casNaMinuty(&ob->zacatek);
    const uint16_t kon = casNaMinuty(&ob->konec);
    if (casVRozsahu(nowMin, zac, kon)) {
      return i;
    }
  }
  return -1;
}

uint8_t zakladniTeplota() {
  const int fromUi = static_cast<int>(uiEez.teplota_vody_set + 0.5f);
  if (fromUi >= 15 && fromUi <= 65) {
    return static_cast<uint8_t>(fromUi);
  }
  return 40;
}

void ulozZakladniStav() {
  s_zakladniTeplota = zakladniTeplota();
  s_zakladniZapnuto = uiEez.sig_chod;
}

void obnovZakladniStav() {
  if (s_zakladniZapnuto) {
    uiBusPlanApplySetpoint(s_zakladniTeplota);
    uiBusPlanApplyStart();
  } else {
    uiBusPlanApplyStop();
  }
}

void aplikujAkci(PlanAkce akce, uint8_t utlumStupne) {
  switch (akce) {
    case PLAN_AKCE_VYP:
      uiBusPlanApplyStop();
      break;
    case PLAN_AKCE_UTLUM: {
      uint8_t cil = s_zakladniTeplota;
      if (utlumStupne > 0 && cil > 15 + utlumStupne) {
        cil = static_cast<uint8_t>(cil - utlumStupne);
      } else if (utlumStupne > 0) {
        cil = 15;
      }
      if (s_zakladniZapnuto) {
        uiBusPlanApplySetpoint(cil);
        uiBusPlanApplyStart();
      }
      break;
    }
    case PLAN_AKCE_NORMAL:
    default:
      obnovZakladniStav();
      break;
  }
}

void aktualizujMonitoring(int aktivniObdobi, PlanAkce akce, uint8_t utlumStupne) {
  if (!g_planConfig.aktivni) {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "PLAN VYPNUTY");
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "Casovy plan je neaktivni");
    uiEez.sig_utlum = false;
    return;
  }

  if (aktivniObdobi < 0) {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "TYDENNI PLAN AKTIVNI");
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "Bezny rezim - mimo planovana obdobi");
    uiEez.sig_utlum = false;
    return;
  }

  const char* obNazev = climatePlanObdobiNazev(static_cast<uint8_t>(aktivniObdobi));

  if (akce == PLAN_AKCE_UTLUM) {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "UTLUM AKTIVNI");
    uiEez.sig_utlum = true;
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "%s: utlum -%u st",
             obNazev, (unsigned)utlumStupne);
  } else if (akce == PLAN_AKCE_VYP) {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "TOPENI VYPNUTO");
    uiEez.sig_utlum = false;
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "%s: topeni vypnuto", obNazev);
  } else {
    snprintf(uiEez.plan_title, sizeof(uiEez.plan_title), "TYDENNI PLAN AKTIVNI");
    uiEez.sig_utlum = false;
    snprintf(uiEez.plan_text, sizeof(uiEez.plan_text), "%s: bezny rezim", obNazev);
  }
}

int denIndexZCasu(const struct tm* tmLocal) {
  const int wday = tmLocal->tm_wday;
  if (wday == 0) {
    return 6;
  }
  return wday - 1;
}

}  // namespace

void climatePlanSetDefaults(void) {
  memset(&g_planConfig, 0, sizeof(g_planConfig));
  g_planConfig.aktivni = false;

  nastavObdobi(&g_planConfig.obdobi[PLAN_OBDOBI_VT1], 5, 0, 8, 0, PLAN_CAS_OD_DELKA);
  nastavObdobi(&g_planConfig.obdobi[PLAN_OBDOBI_VT2], 8, 0, 14, 0, PLAN_CAS_OD_DELKA);
  nastavObdobi(&g_planConfig.obdobi[PLAN_OBDOBI_VT3], 14, 0, 17, 0, PLAN_CAS_OD_DELKA);
  nastavObdobi(&g_planConfig.obdobi[PLAN_OBDOBI_VT4], 17, 0, 21, 0, PLAN_CAS_OD_DELKA);
  // Noc: 8 hodin (22:00 -> 06:00)
  nastavObdobi(&g_planConfig.obdobi[PLAN_OBDOBI_NOC], 22, 0, 6, 0, PLAN_CAS_OD_DELKA);

  for (int d = 0; d < PLAN_POCET_DNU; ++d) {
    for (int o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      nastavBunku(&g_planConfig.tabulka[d][o], PLAN_AKCE_NORMAL, 0);
    }
  }
}

void climatePlanSave(void) {
  storageSavePlanConfig(&g_planConfig);
}

const PlanTydenConfig* climatePlanGetConfig(void) {
  return &g_planConfig;
}

PlanTydenConfig* climatePlanGetConfigMutable(void) {
  return &g_planConfig;
}

const char* climatePlanDenNazev(uint8_t denIndex) {
  if (denIndex >= PLAN_POCET_DNU) {
    return "?";
  }
  return kDny[denIndex];
}

const char* climatePlanObdobiNazev(uint8_t obdobiIndex) {
  if (obdobiIndex >= PLAN_POCET_OBDOBI) {
    return "?";
  }
  return kObdobi[obdobiIndex];
}

const char* climatePlanAkceText(PlanAkce akce, uint8_t utlumStupne) {
  (void)utlumStupne;
  switch (akce) {
    case PLAN_AKCE_UTLUM:
      return "utlum";
    case PLAN_AKCE_VYP:
      return "vypnuto";
    case PLAN_AKCE_NORMAL:
    default:
      return "normal";
  }
}

void climatePlanInit(void) {
  climatePlanSetDefaults();
  const bool loaded = storageLoadPlanConfig(&g_planConfig);
  if (!loaded) {
    climatePlanSetDefaults();
    climatePlanSave();
  }
  // Sanitize vždy — limity délek / buněk; layout se nemění, data z NVS zůstanou.
  for (int o = 0; o < PLAN_POCET_OBDOBI; ++o) {
    g_planConfig.obdobi[o].cas_rezim = PLAN_CAS_OD_DELKA;
    PlanObdobiCas* ob = &g_planConfig.obdobi[o];
    const int z = static_cast<int>(ob->zacatek.hodina) * 60 + ob->zacatek.minuta;
    int k = static_cast<int>(ob->konec.hodina) * 60 + ob->konec.minuta;
    int delka = (k >= z) ? (k - z) : (24 * 60 - z + k);
    const int maxDelka = (o == PLAN_OBDOBI_NOC) ? (8 * 60) : (4 * 60);
    if (delka < 30) {
      delka = 30;
    }
    if (delka > maxDelka) {
      delka = maxDelka;
    }
    k = (z + delka) % (24 * 60);
    ob->konec.hodina = static_cast<uint8_t>(k / 60);
    ob->konec.minuta = static_cast<uint8_t>(k % 60);
  }
  for (int d = 0; d < PLAN_POCET_DNU; ++d) {
    for (int o = 0; o < PLAN_POCET_OBDOBI; ++o) {
      PlanBunka* b = &g_planConfig.tabulka[d][o];
      if (b->akce > PLAN_AKCE_VYP) {
        b->akce = PLAN_AKCE_NORMAL;
        b->utlum_stupne = 0;
      } else if (b->akce == PLAN_AKCE_UTLUM) {
        if (b->utlum_stupne < 1) {
          b->utlum_stupne = 1;
        } else if (b->utlum_stupne > 5) {
          b->utlum_stupne = 5;
        }
      } else {
        b->utlum_stupne = 0;
      }
    }
  }
  s_aplikovaneObdobi = -1;
  s_aplikovanaAkce = PLAN_AKCE_NORMAL;
  s_aplikovanyUtlum = 0;
  s_planOvlada = false;
  aktualizujMonitoring(-1, PLAN_AKCE_NORMAL, 0);
}

int climatePlanAktivniObdobi(void) {
  return s_aplikovaneObdobi;
}

bool climatePlanJeAktivni(void) {
  return g_planConfig.aktivni;
}

bool climatePlanAplikujeUtlum(void) {
  return s_aplikovanaAkce == PLAN_AKCE_UTLUM;
}

void climatePlanTick(void) {
  const uint32_t nowMs = millis();

  if (!g_planConfig.aktivni || !uiEez.cas_platny) {
    if (s_planOvlada) {
      obnovZakladniStav();
      s_planOvlada = false;
      s_aplikovaneObdobi = -1;
      s_aplikovanaAkce = PLAN_AKCE_NORMAL;
      s_aplikovanyUtlum = 0;
    }
    if (nowMs - s_lastMonitorMs >= 1000) {
      s_lastMonitorMs = nowMs;
      aktualizujMonitoring(-1, PLAN_AKCE_NORMAL, 0);
    }
    return;
  }

  time_t now = time(nullptr);
  struct tm tmLocal;
  localtime_r(&now, &tmLocal);
  const uint16_t nowMin =
      static_cast<uint16_t>(tmLocal.tm_hour * 60 + tmLocal.tm_min);
  uint8_t den = static_cast<uint8_t>(denIndexZCasu(&tmLocal));

  const int obdobi = najdiAktivniObdobi(nowMin);
  PlanAkce cilovaAkce = PLAN_AKCE_NORMAL;
  uint8_t cilovyUtlum = 0;

  if (obdobi >= 0) {
    // Období přes půlnoc (typicky Noc): do rána platí buňka dne, kdy období začalo.
    const PlanObdobiCas* ob = &g_planConfig.obdobi[obdobi];
    const uint16_t zac = casNaMinuty(&ob->zacatek);
    const uint16_t kon = casNaMinuty(&ob->konec);
    if (zac > kon && nowMin < kon) {
      den = static_cast<uint8_t>((den + 6) % PLAN_POCET_DNU);
    }
    const PlanBunka* bunka = &g_planConfig.tabulka[den][obdobi];
    cilovaAkce = bunka->akce;
    cilovyUtlum = bunka->utlum_stupne;
  }

  if (obdobi != s_aplikovaneObdobi || cilovaAkce != s_aplikovanaAkce ||
      cilovyUtlum != s_aplikovanyUtlum) {
    if (obdobi < 0) {
      if (s_planOvlada) {
        obnovZakladniStav();
        s_planOvlada = false;
      }
    } else {
      if (!s_planOvlada) {
        ulozZakladniStav();
        s_planOvlada = true;
      }
      aplikujAkci(cilovaAkce, cilovyUtlum);
    }
    s_aplikovaneObdobi = obdobi;
    s_aplikovanaAkce = cilovaAkce;
    s_aplikovanyUtlum = cilovyUtlum;
  }

  if (nowMs - s_lastMonitorMs >= 1000) {
    s_lastMonitorMs = nowMs;
    if (obdobi >= 0) {
      aktualizujMonitoring(obdobi, cilovaAkce, cilovyUtlum);
    } else {
      aktualizujMonitoring(-1, PLAN_AKCE_NORMAL, 0);
    }
  }
}
