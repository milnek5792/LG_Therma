// h2_ble_bridge.ino — Seeed XIAO ESP32-C3: SwitchBot Meter → UART → Tab5
//
// Zapojení na Tab5 M5-Bus:
//   Xiao D6 (TX, GPIO21)  →  Tab5 G7 (RX)
//   Xiao D7 (RX, GPIO20)  ←  Tab5 G6 (TX)
//   GND                   —  GND
//
// PlatformIO: env xiao-esp32c3-bridge (src/main.cpp)
// Arduino IDE: Board = XIAO_ESP32C3, knihovna NimBLE-Arduino

#include <Arduino.h>

// Skutečný firmware je v src/main.cpp (PlatformIO).
