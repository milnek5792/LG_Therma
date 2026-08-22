// lg_lin_api.h — verejne API sbernice (volatelne z UI jadra)
#ifndef LG_LIN_API_H
#define LG_LIN_API_H

#include <Arduino.h>

void lgBusInit();
void lgBusTick();

unsigned long lgPocetPaketu();
unsigned long lgPocetRxBajtu();
bool lgBusIsReady();
bool lgZapisBezi();

void nastavMonitorPozastaven(bool pauza);
void prepniMonitorPozastaven();
void prepniSoloRezim();
void prepniParallelRezim();
void prepniDrzetStav();  // alias → prepniParallelRezim

void lgNastavDrzenyStav(uint8_t teplota, bool zapnuto);
void lgZrusCekaniOrig(const char* duvod);
void lgUkonciCekaniProStop();
void provedZapisTeploty(uint8_t teplota, bool zapnuto, bool zmenaStartu);

#endif
