// h2_ble_bridge.ino — ESP32-C3: SwitchBot Meter → UART → Tab5
//
// Zapojení na Tab5 M5-Bus:
//   C3 TX GPIO21  →  Tab5 G7 (RX)
//   C3 RX GPIO20  ←  Tab5 G6 (TX)
//   GND           —  GND
//
// PlatformIO: env esp32-c3-bridge (src/main.cpp)
// Arduino IDE: Board = ESP32C3 Dev Module, knihovna NimBLE-Arduino

#include <Arduino.h>

// Skutečný firmware je v src/main.cpp (PlatformIO).
