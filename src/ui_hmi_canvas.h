#ifndef UI_HMI_CANVAS_H
#define UI_HMI_CANVAS_H

#include <lgfx/v1/LGFX_Sprite.hpp>

bool uiHmiCanvasInit();
bool uiHmiCanvasReady();
lgfx::LovyanGFX& uiHmiCanvas();
int uiHmiCanvasFlushBandHeight();
bool uiHmiCanvasFlushBandAt(int y);

#endif
