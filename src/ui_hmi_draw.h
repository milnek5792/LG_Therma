#ifndef UI_HMI_DRAW_H
#define UI_HMI_DRAW_H

void uiHmiDrawInit();
void uiHmiDrawTick();
bool uiHmiDrawShouldStartUi();
bool uiHmiDrawShouldStartLin();
int uiHmiDrawLoopDelayMs();

#endif
