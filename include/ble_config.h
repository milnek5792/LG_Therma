// ble_config.h — SwitchBot Meter (pokoj + venkovní, Wi‑Fi↔BLE na ESP32-S3 7B)
#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#ifndef LG_THERMA_BLE_ROOM
#define LG_THERMA_BLE_ROOM 1
#endif

// Pokojový SwitchBot Meter
#ifndef BLE_METER_MAC
#define BLE_METER_MAC "EC:6F:03:86:1E:6B"
#endif

// Venkovní SwitchBot Meter
#ifndef BLE_OUTDOOR_MAC
#define BLE_OUTDOOR_MAC "E8:76:C3:46:66:14"
#endif

// 0 = jen nakonfigurované MAC (produkce); 1 = první SwitchBot s T (diag)
#ifndef BLE_ACCEPT_ANY_SWITCHBOT
#define BLE_ACCEPT_ANY_SWITCHBOT 0
#endif

#ifndef BLE_POLL_INTERVAL_MS
#define BLE_POLL_INTERVAL_MS 120000
#endif

/** Opakování po neúspěšném scanu / MISS (ms). */
#ifndef BLE_FAIL_RETRY_MS
#define BLE_FAIL_RETRY_MS 20000u
#endif

/** Max. rychlých retry po MISS, pak zase plný interval. */
#ifndef BLE_MISS_RETRY_MAX
#define BLE_MISS_RETRY_MAX 3
#endif

// Meter vysílá 1–4 s — 10 s scan stačí (oba senzory v jednom cyklu)
#ifndef BLE_SCAN_MS
#define BLE_SCAN_MS 10000
#endif

// Celý poll (scan + cleanup) max
#ifndef BLE_POLL_WATCHDOG_MS
#define BLE_POLL_WATCHDOG_MS 35000
#endif

#endif
