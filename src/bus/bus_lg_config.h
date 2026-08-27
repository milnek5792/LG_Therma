// bus_lg_config.h — LIN na Waveshare ESP32-S3-Touch-LCD-7B
#ifndef LG_CONFIG_H
#define LG_CONFIG_H

// LIN: UART header RX=44 TX=43, DIP=UART2
// Upload: USB-C "UART" CH343, DIP=UART1
// Monitor: USB-C "USB" native CDC (EXIO5=0), nezávislé na DIP
#ifndef LG_MBUS_RX_PIN
#define LG_MBUS_RX_PIN 44
#endif
#ifndef LG_MBUS_TX_PIN
#define LG_MBUS_TX_PIN 43
#endif
#ifndef LG_UART_NUM
#define LG_UART_NUM 2
#endif

#ifndef LG_BAUDRATE
#define LG_BAUDRATE 300
#endif

#ifndef LG_LIN_IN_LOOP
#define LG_LIN_IN_LOOP 1
#endif

/** 0 = UART hned (doporučeno). Dřív 1500 ms — promeškalo A0 a čekalo se na další cyklus. */
#ifndef LG_DEFER_LIN_START
#define LG_DEFER_LIN_START 0
#endif
#ifndef LG_LIN_START_DELAY_MS
#define LG_LIN_START_DELAY_MS 0
#endif

#ifndef LG_KRATKY_LOG
#define LG_KRATKY_LOG 1
#endif

/** A0 mladší než toto = LIN online (UI + MQTT). Default 1 min. */
#ifndef LG_A0_FRESH_MS
#define LG_A0_FRESH_MS 60000u
#endif

/**
 * 1 = linTask jen tichý parse (bez plných Serial dumpů).
 * Plný dump přes USB CDC blokuje linTask a vypadá to jako „LIN umřel“.
 */
#ifndef LG_LIN_QUIET_PARSE
#define LG_LIN_QUIET_PARSE 1
#endif

/** Priorita linTask (Arduino loop = 1, net = 1). LIN musí být nejvýš na core 1. */
#ifndef LG_LIN_TASK_PRIO
#define LG_LIN_TASK_PRIO 12
#endif

/** Typická perioda A0 v klidu (TČ VYP) — z monitoru ~10–25 s. */
#ifndef LG_A0_SLOW_PERIOD_MS
#define LG_A0_SLOW_PERIOD_MS 25000u
#endif

/** Oranžová → červená až po jednom LIN cyklu + rezerva (ms). */
#ifndef UI_SP_PENDING_MARGIN_MS
#define UI_SP_PENDING_MARGIN_MS 5000u
#endif

#endif
