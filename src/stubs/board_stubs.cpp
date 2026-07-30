// Stubs for remaining bring-up (SDIO arbiter / Tab5 touch helpers)
#include "net_sdio_arbiter.h"
#include "ui_touch_tab5.h"

void netSdioInit(void) {}
void netSdioTick(void) {}
bool netSdioCanBle(void) { return true; }
bool netSdioTryBeginBle(void) { return true; }
void netSdioEndBle(void) {}
bool netSdioMqttWanted(void) { return false; }
bool netSdioCanMqtt(void) { return true; }
void netSdioBumpWatch(uint32_t holdMs) { (void)holdMs; }
void netSdioBumpWatchDefault(void) {}
void netSdioWatchOff(void) {}
void netSdioSetMqttSession(bool online) { (void)online; }
void netSdioBeginUiHeavy(uint32_t holdMs) { (void)holdMs; }
void netSdioBeginUiHeavyDefault(void) {}
bool netSdioTlsBusy(void) { return false; }

void uiTouchTab5Init(void) {}
void uiTouchTab5SetupCheck(void) {}
void uiTouchVisualInit(void) {}
void uiTouchVisualSync(void) {}
