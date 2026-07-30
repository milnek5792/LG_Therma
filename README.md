# LG Therma 7B (PlatformIO / pioarduino)

Waveshare **ESP32-S3-Touch-LCD-7B** port of LG Therma HMI.

## Status

- Board drivers: RGB LCD + IO extension (CH32V003) + GT911 (from Waveshare demo)
- LVGL 9.2 + EEZ UI scaled **1280×720 → 1024×600** (`ui_panel_scale`)
- Graphics-first: Wi‑Fi / MQTT / LIN are stubs for now

## Build

```bash
pio run -e waveshare-s3-7b
pio run -e waveshare-s3-7b -t upload
pio device monitor
```

Board: ESP32-S3 N16R8, `qio_opi` PSRAM, `huge_app` partition.
