// bus_lg_config.h — LIN na Waveshare ESP32-S3-Touch-LCD-7B
#ifndef LG_CONFIG_H
#define LG_CONFIG_H

// UART header (TTL): RX=44 TX=43 — DIP přepínač na UART2
// Flash: USB-C "UART" + DIP UART1 (CH343). Logy: USB-C "USB" TinyUSB CDC (EXIO5=0).
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

// Odposlech v loop() vedle LVGL (ne v net tasku)
#ifndef LG_LIN_IN_LOOP
#define LG_LIN_IN_LOOP 1
#endif

#ifndef LG_DEFER_LIN_START
#define LG_DEFER_LIN_START 1
#endif
#ifndef LG_LIN_START_DELAY_MS
#define LG_LIN_START_DELAY_MS 1500
#endif

// Kratší log A0 (méně spamů na USB CDC)
#ifndef LG_KRATKY_LOG
#define LG_KRATKY_LOG 1
#endif

#endif
