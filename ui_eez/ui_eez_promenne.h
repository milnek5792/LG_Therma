// ui_eez_promenne.h — navod pro EEZ Studio (jen dokumentace, nekomiluje se)
//
// LG Therma V — seznam promennych pro EEZ Studio (Native variables)
// ================================================================
// Project -> Variables -> Add. Jmena a typy presne podle tabulky.
// Firmware: ui_eez_model.h (EEZ_VAR_* makra)
//
// TEPLOTY (Float nebo Integer, jednotka °C)
// -----------------------------------------
// teplota_vody_set      pozadovana teplota vody (setpoint)
// teplota_vody_vstup    teplota vody na vstupu T/C
// teplota_vody_vystup   teplota vody na vystupu T/C
// teplota_vnitrni       pokojova / BLE (zatim ---)
// teplota_venkovni      venkovni (zatim ---, budouci senzor)
// teplota_spad          |vystup - vstup| Delta T
//
// REZIMY (Integer — enum)
// -----------------------
// rezim                 0 = Auto (adaptivni/pokoj)
//                       1 = Vystupni teplota (primy setpoint vody)
// stav_tc               0 = Vyp
//                       1 = Cekam orig.
//                       2 = Prestart
//                       3 = Beh
//
// SIGNALKY (Boolean — LED / ikona)
// --------------------------------
// sig_chod              T/C v provozu (PRESTART nebo beh)
// sig_cerpadlo          obehove cerpadlo
// sig_kompresor         kompresor (stabilni beh B3=0x0A)
// sig_el_topeni         el. patrona / pridavne topeni
// sig_odmrazovani       odmrazovaci cyklus
// sig_wifi              WiFi pripojeno (ESP32-C6)
// sig_mqtt              MQTT broker OK
// sig_ble               BLE pokojovy senzor OK
// sig_utlum             aktivni casovy utlum (planovac)
// sig_alarm             porucha / chyba
//
// SIGNALKY S IKONOU (EEZ Imgbutton + Bitmaps)
// -------------------------------------------
// Doporuceny widget: Imgbutton (ne klikaci indikator)
//   Released image         = ic_<jmeno>_off   (sediva / neaktivni)
//   Checked released image = ic_<jmeno>_on    (barevna / aktivni)
//   Checked state type     = Expression
//   Checked                = sig_<jmeno>       (boolean promenna)
//   Clickable              = false
//   Name widgetu           = ico_<jmeno>       (volitelne, pro export)
//
// PNG: 32x32 nebo 48x48, pruhledne pozadi, jednotny styl (outline NEBO filled).
// Bitmaps panel: Project -> Bitmaps -> + -> import PNG.
//
// Tabulka bitmap pro vsechny sig_* (EEZ -> Bitmaps -> Name):
//
//   promenna          bitmap OFF          bitmap ON           barva ON    popisek Label
//   ----------------  ------------------  ------------------  ----------  ------------------
//   sig_chod          ic_chod_off         ic_chod_on          0x00FF00    Chod T/C
//   sig_cerpadlo      ic_cerpadlo_off     ic_cerpadlo_on      0x00FF00    Cerpadlo
//   sig_kompresor     ic_kompresor_off    ic_kompresor_on     0x00FF00    Kompresor
//   sig_el_topeni     ic_el_topeni_off    ic_el_topeni_on     0xFF8800    El. topeni
//   sig_odmrazovani   ic_odmrazovani_off  ic_odmrazovani_on   0x00AAFF    Odmrazovani
//   sig_wifi          ic_wifi_off         ic_wifi_on          0x0088FF    WiFi
//   sig_mqtt          ic_mqtt_off         ic_mqtt_on          0x0088FF    MQTT
//   sig_ble           ic_ble_off          ic_ble_on           0x0088FF    BLE
//   sig_utlum         ic_utlum_off        ic_utlum_on         0xFFAA00    Utlum
//   sig_alarm         ic_alarm_off        ic_alarm_on         0xFF0000    Alarm
//
// OFF ikony: seda (#666666 nebo #888888), nizky kontrast.
// ON ikony: stejny symbol, barva podle sloupce (viz tabulka).
// Tip: sdileny symbol OFF (stejna seda silueta) + u kazdeho signalu jina ON barva.
//
// Doporucene motivy ikon (Material / Flaticon — stejny styl u vsech):
//   ic_chod         play / power / heat pump running
//   ic_cerpadlo     water pump / circulation
//   ic_kompresor    compressor / fan
//   ic_el_topeni    lightning / heating element
//   ic_odmrazovani  snowflake / defrost
//   ic_wifi         wifi arcs
//   ic_mqtt         cloud / antenna
//   ic_ble          bluetooth
//   ic_utlum        moon / clock / minus
//   ic_alarm        warning triangle / bell
//
// Alternativa bez Imgbutton — dva Image widgety na sobe (stejna pozice):
//   img ON:  Hidden type Expression, Hidden = !sig_wifi
//   img OFF: Hidden type Expression, Hidden = sig_wifi
//
// Alternativa jednoducha — widget Led (bez symbolu):
//   Color type Expression,       Color = sig_cerpadlo ? 0x00FF00 : 0x333333
//   Brightness type Expression,  Brightness = sig_cerpadlo ? 255 : 50
//
// User Widget: oznac Imgbutton + Label -> Create User Widget -> signalka_wifi
//   Kopirovat na panel, u kazde kopie zmenit bitmapy + Checked + text labelu.
//
// CESKY FONT (diakritika v EEZ / LVGL)
// ------------------------------------
// Problem: vychozi LVGL font (Montserrat) NEMA ceskou diakritiku -> prazdne ctverce.
// Reseni: vlastni font z TTF v EEZ -> Fonts panel (Settings -> General -> Fonts zapnuto).
//
// 1) Vyber TTF (free, dobra latinka):
//    Roboto-Regular.ttf, Montserrat-Regular.ttf, DejaVuSans.ttf
//    (Google Fonts / fonts.google.com, licence SIL Open Font)
//
// 2) Fonts -> + -> Add:
//    Name:           font_cs_24          (a dalsi velikosti: font_cs_18, font_cs_32…)
//    Font file:      Roboto-Regular.ttf
//    Bits per pixel: 4                   (4 = kompromis; 8 = hezci, vetsi flash)
//    Font size:      24                  (px — podle widgetu)
//    Ranges:         0x20-0x7F,0xA0-0x17F
//                    (ASCII + latin-1 + latin extended A = cela cestina)
//    Symbols:        (volitelne misto ranges — jen znaky co opravdu pouzivas, mensi flash)
//                    °C0123456789.:+-
//                    AÁBCČDĎEÉĚFGHIÍJKLMNŇOPQRSTŤÚŮVWXYZÝŽ
//                    aábcčdďeéěfghiíjklmnňopqrstuúůvwxyzýž
//
// 3) Po vytvoreni zkontroluj tabulku znaku — musi tam byt e, s, c, r, z, u s hacky/carkou.
//    Chybi znak? Fonts -> font_cs_24 -> Add Characters -> doplň do Symbols.
//
// 4) Styles -> novy styl (Label) -> Text -> Font = font_cs_24
//    U vsech Labelu: Style -> Use style = tento styl.
//    Nebo local style -> Font = font_cs_24.
//
// 5) Velikosti — nevyrabej jeden obri font; radsi 2-3:
//    font_cs_18  popisky, signalky
//    font_cs_28  teploty
//    font_cs_36  hlavni setpoint
//
// 6) Texty v EEZ a ve firmware:
//    - pouzivej UTF-8 s hotovymi znaky: "Cílová voda" (NE rozlozene e + carka)
//    - LVGL vyzaduje NFC tvar (jeden Unicode znak na pismeno)
//    - retezce v C: u8"Cílová voda" nebo L"..." dle exportu
//
// 7) Flash (Tab5): 3 fonty @ bpp 4, range 0xA0-0x17F ~ par set KB — v pohode.
//    Usetri: Symbols jen z textu UI misto celeho range.
//    Usetri: bpp 2 u malych popisku (18 px).
//
// 8) Alternativa (nedoporuceno): ASCII bez diakritiky jako ui_text_ui.h — funguje, ale horsi UX.
//
// Doporucene popisky s diakritikou (EEZ Label / firmware):
//   Cílová voda, Vstupní voda, Výstupní voda, Venkovní vzduch, Pokojová teplota
//   Oběhové čerpadlo, Odmrazování, Elektrické topení, Útlum, Čekám na orig.
//   Zapnout, Vypnout, Storno
//
// CAS (String + Boolean)
// ----------------------
// cas_text              "14:32"
// datum_text            "20.07.2026"
// cas_platny            true = NTP synchronizovan
// plan_text             radek tydenniho planu (HMI widget)
//
// AKCE TLACITTEK (Integer — EEZ -> firmware)
// -------------------------------------------
// akce_tlacitko         0 = zadna
//                       1 = Start/Stop
//                       2 = teplota +
//                       3 = teplota -
//                       4 = prepni rezim Auto/Vystupni
//
// V EEZ u tlacitka: Set Variable akce_tlacitko = 1 (firmware pak vynuluje)
//
// Btn Start/Stop  -> akce_tlacitko = 1
// Btn +           -> akce_tlacitko = 2
// Btn -           -> akce_tlacitko = 3
// Btn Rezim       -> akce_tlacitko = 4
//
// IMPORT PROMENNYCH DO EEZ (hromadne)
// -----------------------------------
// A) SCHranka EEZ (doporuceno) — Ctrl+C / Ctrl+V uvnitr EEZ Studio
//    1. Otevri ui_eez/LG_Therma_promenne.eez-project (22 promennych)
//    2. Project -> Variables, oznac vsechny (Ctrl+A)
//    3. Ctrl+C
//    4. Prejdi do sveho LG_Therma projektu -> Variables -> Ctrl+V
//    Alternativa: Copy to Scrapbook / Paste from Scrapbook (stejny princip)
//    Pozn.: nejde vlozit obycejny text z Poznamkoveho bloku — jen EEZ objekty
//
// B) Rucne: Variables -> + (22x podle seznamu vyse)
//
// C) JSON merge: ui_eez_import_promenne.json + rucni edit .eez-project
//    (viz komentare v predchozi verzi — jen kdyz schranka nefunguje)
//
// HMI PROJEKT Z HTML NAVRHU
// -------------------------
// Soubor: ui_eez/LG_Therma_HMI.eez-project (1280x720, LVGL 9, bez Flow)
// Generator: ui_eez/gen_hmi_eez_project.py (po uprave layoutu znovu spustit)
//
// Otevri v EEZ Studio -> uprav vizualne -> Build -> export do ui_eez/export/
//
// Layout (odpovida HTML mockupu):
//   horni lista: cas, Wi-Fi/MQTT status, Tichy rezim
//   leva zona: +/- setpoint, planovac (plan_text)
//   prava zona: RUN/STOP, 5 signal LED (sig_chod, cerpadlo, kompresor…)
//   spodni lista: 4 telemetry + MENU
//
// EEZ User Actions (implementovano v ui_eez_actions.cpp):
//   akce_teplota_plus   -> uiEez.akce_tlacitko = UI_AKCE_TEPLOTA_PLUS
//   akce_teplota_minus  -> uiEez.akce_tlacitko = UI_AKCE_TEPLOTA_MINUS
//   akce_start_stop     -> uiEez.akce_tlacitko = UI_AKCE_START_STOP
//   akce_tichy_rezim    -> zatim placeholder (budouci utlum)
//   akce_menu           -> zatim placeholder
//
// Po otevreni projektu v EEZ:
//   1. Fonts -> pridej font_cs_* (viz sekce CESKY FONT vyse)
//   2. Prepin popisky na ceske s diakritikou
//   3. Volitelne: ikony misto LED (Imgbutton)
//
// FIRMWARE — NAHRANI NA TAB5
// --------------------------
// Struktura sketchu:
//   koren/  LG_Therma.ino, lv_conf.h, bus_task_*, ui_task_*  (zalozky = jadra)
//   src/    bus, ui, eez export, fonty, sit, klima …
//   ui_eez/ EEZ projekt + export + skripty
//
// 1. Arduino IDE 2.x: knihovna LVGL 9.2.x (NE 9.5 — EEZ export je pro 9.2.2)
//    Library Manager -> lvgl -> vyber verzi 9.2.2
// 2. Po uprave HMI v EEZ Studio:
//      EEZ Studio.exe --build-project ui_eez\LG_Therma_HMI.eez-project
//      python ui_eez\sync_export_to_sketch.py
// 3. bus_lg_config.h: LG_USE_EEZ_LVGL 1 (default)
// 4. Nahraj LG_Therma.ino na Tab5
//
// Propojeni (slozka src/):
//   ui_eez_vars.cpp    — get/set promenne + EEZ expression helpery -> uiEez
//   ui_eez_actions.cpp — tlacitka -> uiEez.akce_tlacitko
//   ui_lvgl_port.cpp   — LVGL init/flush/touch (1280x720)
//
// Fonty: src/ui_eez_font_cs_16.cpp + _24.cpp (lv_font_conv)
// Regenerace: .\ui_eez\gen_fonts.ps1
// Barvy spatne? zkus v lv_conf.h LV_COLOR_16_SWAP 1
//
#ifndef UI_EEZ_PROMENNE_H
#define UI_EEZ_PROMENNE_H
#endif
