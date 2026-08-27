#ifndef UI_EEZ_SIGNAL_LEDS_H
#define UI_EEZ_SIGNAL_LEDS_H

void uiEezInitSignalLeds(void);
void uiEezApplySignalLeds(void);
/** Vynutit překreslení MAN/PID/EKV (např. po návratu na hlavní obrazovku). */
void uiEezRefreshRegStatus(void);

#endif
