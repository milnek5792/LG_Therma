# LG Therma 7B (PlatformIO / pioarduino)

Waveshare **ESP32-S3-Touch-LCD-7B** port of LG Therma HMI.

## Status

- Board: RGB LCD + IO extension (CH32V003) + GT911
- UI: LVGL 9.2 + EEZ scaled **1280×720 → 1024×600**
- LIN: LG protokol přes UART header (DIP UART2)
- Síť: Wi‑Fi, NTP, MQTT TLS (EMQX), watch + telemetrie
- BLE: 2× SwitchBot (pokoj + venkovní MAC v `ble_config.h`)
- PID Auto + ekviterm (0–100 % → SP vody 25–45 °C) — viz [docs/PLAN.md](docs/PLAN.md)
- PWA: plánováno

## Plán projektu

Kompletní etapy a milníky: **[docs/PLAN.md](docs/PLAN.md)**

| Etapa | Popis | Stav |
|-------|--------|------|
| 1 | LG protokol / LIN | hotovo |
| 2 | UI na 7B | hotovo |
| 3 | Wi‑Fi, NTP, MQTT | hotovo |
| 4 | BLE teploměry (pokoj + venkovní) | hotovo (doplnit outdoor MAC) |
| 5 | Adaptivní PID + ekviterm | hotovo ve FW |
| 6 | PWA dálkové ovládání | plánováno |

## Build

```bash
pio run -e waveshare-s3-7b
pio run -e waveshare-s3-7b -t upload
pio device monitor
```

Board: ESP32-S3 N16R8, `qio_opi` PSRAM, `huge_app` partition.

**Flash:** CH343, DIP UART1 (`upload_port` v `platformio.ini`, typicky COM9)  
**Monitor:** native USB CDC, DIP UART2 (`monitor_port`, typicky COM10)

Po flashi v logu ověř `FW-ID=2026-08-03b` a `SUB ok lgtherma/cmd/compressor` (ne `cmd/quiet`).
