# LG Therma 7B — plán projektu

**Cíl:** HMI pro LG Therma TČ na desce **Waveshare ESP32-S3-Touch-LCD-7B** (1024×600), build přes **PlatformIO / pioarduino**, vzdálené ovládání přes MQTT a později PWA.

**Původní projekt:** `LG_Therma` (M5Stack Tab5) — HMI + LIN hotové; migrace na 7B kvůli nativnímu Wi‑Fi/BLE bez SDIO problémů Tab5.

---

## Etapy

### 1 — Dekódování a aplikace LG protokolu přes LIN

**Rozsah:** dekódování A0/C0, stav TČ, čerpadlo/kompresor, setpoint, zápis přes frontu na sběrnici.

**Moduly:** `src/bus/bus_lg_*`, `src/ui/ui_bus_bindings.*`

**Hotovo když:** A0 na UI, START/STOP a setpoint fungují spolehlivě přes LIN.

| Stav | Poznámka |
|------|----------|
| ✅ Hotovo | LIN na UART header (DIP UART2), model + mutex, protokol B2/B3 |
| ✅ Hotovo | UI vázané na bus, detekce kompresoru (`lgJeKompresorBezi`) |
| 🔲 Zbývá | tichý režim LIN, časový plánovač, edge cases |

---

### 2 — UI rozhraní na 7B

**Rozsah:** EEZ/LVGL 1024×600, touch, signálky, nastavení, Wi‑Fi/MQTT formuláře.

**Moduly:** `src/ui/*`, `src/board/*`, `tools/sync_eez_export_7b.py`

**Hotovo když:** všechny obrazovky použitelné, touch sedí, žádné Tab5 závislosti.

| Stav | Poznámka |
|------|----------|
| ✅ Hotovo | RGB LCD + GT911 + CH32V003 IO (Waveshare 7B demo) |
| ✅ Hotovo | EEZ škálování **1280×720 → 1024×600** (`ui_panel_scale`) |
| ✅ Hotovo | Hlavní obrazovka, settings, signálky (Wi‑Fi, MQTT, BLE, vzdálené sledování) |
| 🔲 Zbývá | doladění touch, úklid stubů (`src/stubs/`), aktualizace README |

---

### 3 — Wi‑Fi, NTP, MQTT protokol a připojení k brokeru

**Rozsah:** EMQX TLS, telemetrie (retain), příkazy, watch režim pro mobilní panel.

**Moduly:** `src/net/*`, `include/mqtt_config.h`

**Hotovo když:** watch ON → tele stabilně >10 min, cmd power/setpoint fungují.

| Stav | Poznámka |
|------|----------|
| ✅ Hotovo | Wi‑Fi + NVS (`net_wifi_mgr`), NTP (`net_ntp_time`) |
| ✅ Hotovo | MQTT TLS PubSubClient, topic kořen `lgtherma/` |
| ✅ Hotovo | `cmd/watch` → tele jen při aktivním sledování |
| ✅ Hotovo | `tele/compressor`, `tele/lin`, `tele/pump`, teploty, `cmd/power`, `cmd/setpoint` |
| 🔲 Zbývá | merge do `main`, stabilizace reconnect, citlivé údaje mimo git |

**Workflow flash / monitor:**
- Upload: CH343, DIP **UART1**, typicky `COM9`
- Monitor: native USB CDC, DIP **UART2**, typicky `COM10`, `monitor_dtr/rts=0`

---

### 4 — Připojení Bluetooth teploměru

**Rozsah:** pokojová + venkovní teplota (2× SwitchBot Meter, stejný protokol), UI a MQTT `tele/temp_room` / `tele/temp_outdoor`.

**Moduly:** `src/net/climate_ble_room.*`, `include/ble_config.h` (`BLE_METER_MAC`, `BLE_OUTDOOR_MAC`)

**Hotovo když:** oba senzory se čtou v jednom scan cyklu; `sig_ble` = pokoj OK.

| Stav | Poznámka |
|------|----------|
| ✅ Hotovo | Dual SwitchBot scan (Wi‑Fi suspend → NimBLE → resume) |
| ✅ Hotovo | `teplota_vnitrni` / `teplota_venkovni` z BLE (ne LIN A0 B5) |
| 🔲 Zbývá | doplnit reálnou `BLE_OUTDOOR_MAC`, dlouhodobá spolehlivost |

---

### 5 — Adaptivní PID termostat

**Rozsah:** režim Auto — ekviterm (venkovní BLE) + PID trim podle pokoje; akční veličina **0–100 %** mapovaná na SP vody **25–45 °C**; obrazovka grafu/PID.

**Moduly:** `src/climate/climate_regulator.*`, `src/ui/ui_eez_regulator.*`, NVS `reg_cfg`, hook v `uiBusBindingsTick`

**Hotovo když:** Auto drží pokojovou T (default 22 °C), plán UTLUM snižuje pokojový SP, VYP stopuje.

| Stav | Poznámka |
|------|----------|
| ✅ Hotovo | Ekviterm −15 °C→100 % / 15 °C→0 %, PID trim ±25 %, bias adaptace |
| ✅ Hotovo | Režim **PID only** (bez ekvitermy / venkovního T) — testování |
| ✅ Hotovo | Auto: ± / MQTT = pokojový SP; ruční = voda 25–45 |
| ✅ Hotovo | Obrazovka Nastavení → Regulator (graf + Kp/Ki/Kd/křivka) |
| 🔲 Zbývá | ladění zisků na reálném TČ, outdoor MAC |

---

### 6 — PWA aplikace pro dálkové ovládání

**Rozsah:** mobilní/web panel — watch, telemetrie, START/STOP, setpoint; náhrada za IoT Panel / MQTTX.

**Backend:** EMQX + retain tele (již na desce).

**Hotovo když:** PWA ovládá watch/power/setpoint bez externích nástrojů.

| Stav | Poznámka |
|------|----------|
| 🟡 Částečně | MQTT topic schéma hotové, IoT Panel ověřený |
| 🔲 Zbývá | vlastní PWA (React/Vue/vanilla), auth, UX pro watch/retain |

---

## Milníky (pořadí)

| # | Etapa | Milník |
|---|--------|--------|
| M1 | 1 + 2 | LIN na UI, touch OK |
| M2 | 3 | MQTT watch + tele + cmd stabilní |
| M3 | 4 | pokojová teplota z BLE na panelu |
| M4 | 5 | adaptivní PID Auto |
| M5 | 6 | PWA pro vzdálené ovládání |

**Aktuální pozice (2026-08):** M1–M3 hotové (outdoor MAC doplnit), M4 implementováno ve FW, M5 před námi.

---

## MQTT topic schéma (shrnutí)

Kořen: `lgtherma/`

| Směr | Topic | Payload |
|------|--------|---------|
| tele | `tele/temp_room`, `tele/temp_inlet`, `tele/temp_outlet`, `tele/temp_set`, … | číslo / `ON`/`OFF` / `___` |
| tele | `tele/compressor`, `tele/pump`, `tele/power`, `tele/lin` | `ON` / `OFF` |
| tele | `tele/watch` | `ON` / `OFF` |
| cmd | `cmd/watch` | `ON` / `OFF` (retain při otevření panelu) |
| cmd | `cmd/power`, `cmd/setpoint`, `cmd/mode` | viz `include/mqtt_config.h` |
| test | `cmd/compressor` | `ON` / `OFF` — simulace kompresoru (bez reálného náběhu) |

Telemetrie běží jen při aktivním **watch** (`MQTT_TELE_REQUIRE_WATCH=1`).

---

## Technické poznámky (7B vs Tab5)

| | Tab5 | 7B |
|---|------|-----|
| Wi‑Fi / BLE | SDIO přes C6, arbitraž | nativní na S3 |
| Displej | M5 canvas | RGB 1024×600 + LVGL 9.2 |
| Upload | — | CH343 UART1 |
| Monitor | — | USB CDC (COM číslo může skákat) |

Z Tab5 projektu **nezavádět:** `net_sdio_arbiter`, suspend UI při TLS, M5Unified.

---

## Související repozitáře / větve

- **Tento repo:** `LG_Therma_7B` — aktivní vývoj na Waveshare 7B
- **Původní:** `LG_Therma` — Tab5, referenční LIN/HMI/PID nápady
- **Větev:** `feat/mqtt-watch-compressor` — MQTT watch + tele + kompresor
