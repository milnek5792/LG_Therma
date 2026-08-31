#include "ui_eez_settings.h"

#include "lg_board.h"
#include "app_version.h"
#include "ui_display_mgr.h"
#include "ui_eez_actions.h"
#include "ui_eez_fonts.h"
#include "ui_eez_model.h"
#include "ui_eez_vars.h"
#include "climate_room_uart.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

settings_objects_t settingsObj;

namespace {

constexpr uint32_t kColBg = 0x121214u;
constexpr uint32_t kColPanel = 0x1A1A1Fu;
constexpr uint32_t kColBorder = 0x24242Bu;
constexpr uint32_t kColText = 0xE0E0E6u;
constexpr uint32_t kColMuted = 0x8E8E93u;
constexpr uint32_t kColAccent = 0x0A84FFu;
constexpr uint32_t kColGreen = 0x30D158u;
constexpr uint32_t kColOrange = 0xFF9F0Au;
constexpr uint32_t kColDisabled = 0x2C2C2Eu;
constexpr uint32_t kColPurple = 0x5856D6u;

constexpr int kW = BOARD_PANEL_W;
constexpr int kH = BOARD_PANEL_H;
constexpr int kMargin = 10;
constexpr int kGap = 10;
constexpr int kHeaderH = 50;
constexpr int kBtnH = 40;
constexpr int kLine = 32;
constexpr int kPad = 10;
constexpr int kTitleY = 6;
constexpr int kBodyY = 44;

const lv_font_t* kFont = &ui_font_font_cs_24;
const lv_font_t* kFontTitle = &ui_font_font_cs_24;

void setLabelIfChanged(lv_obj_t* lbl, const char* text) {
  if (!lbl || !text) {
    return;
  }
  const char* prev = lv_label_get_text(lbl);
  if (prev && strcmp(prev, text) == 0) {
    return;
  }
  lv_label_set_text(lbl, text);
}

lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(obj, lv_color_hex(kColPanel), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(obj, lv_color_hex(kColBorder), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, int maxW, const char* text, uint32_t color) {
  lv_obj_t* obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  if (maxW > 0) {
    lv_obj_set_width(obj, maxW);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
  }
  lv_obj_set_style_text_font(obj, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(obj, text);
  return obj;
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text, lv_event_cb_t cb, uint32_t bg) {
  lv_obj_t* obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(obj);
  lv_obj_set_style_text_font(lbl, kFont, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFu), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

void styleToggleBtn(lv_obj_t* btn, bool on) {
  if (!btn) {
    return;
  }
  const void* prev = lv_obj_get_user_data(btn);
  const uintptr_t state = on ? 1u : 2u;
  if (reinterpret_cast<uintptr_t>(prev) == state) {
    return;
  }
  lv_obj_set_user_data(btn, reinterpret_cast<void*>(state));
  lv_obj_set_style_bg_color(
      btn, lv_color_hex(on ? kColGreen : 0x48484Au), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, on ? "Zapnuto" : "Vypnuto");
  }
}

void styleConnectBtn(lv_obj_t* btn, bool enabled, bool connected) {
  if (!btn) {
    return;
  }
  const uintptr_t state = connected ? 3u : (enabled ? 2u : 1u);
  const void* prev = lv_obj_get_user_data(btn);
  if (reinterpret_cast<uintptr_t>(prev) == state) {
    return;
  }
  lv_obj_set_user_data(btn, reinterpret_cast<void*>(state));
  const uint32_t bg = connected ? kColGreen : (enabled ? kColAccent : kColDisabled);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, (enabled || connected) ? 255 : 180, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) {
    setLabelIfChanged(lbl, connected ? "MQTT OK" : "Připojit MQTT");
  }
}

void setTextColorCached(lv_obj_t* obj, uint32_t color) {
  if (!obj) {
    return;
  }
  const uintptr_t want = color;
  if (reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj)) == want) {
    return;
  }
  lv_obj_set_user_data(obj, reinterpret_cast<void*>(want));
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void updateBrightnessLabel() {
  if (!settingsObj.lbl_brightness) {
    return;
  }
  char line[24];
  snprintf(line, sizeof(line), "Jas %u%%", (unsigned)uiDisplayGetBrightness());
  setLabelIfChanged(settingsObj.lbl_brightness, line);
}

void updateMeterButtons() {
  const int n = climateRoomFoundCount();
  auto styleMeterBtn = [](lv_obj_t* btn, uint8_t idx, int foundCount) {
    if (!btn) {
      return;
    }
    if ((int)idx > foundCount) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
    ClimateRoomFound f{};
    char line[24];
    if (climateRoomGetFound(idx, &f) && f.valid) {
      snprintf(line, sizeof(line), "Tepl.%u %.0f °C", (unsigned)idx, (double)f.temp_c);
    } else {
      snprintf(line, sizeof(line), "Tepl.%u", (unsigned)idx);
    }
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
      lv_label_set_text(lbl, line);
    }
  };
  styleMeterBtn(settingsObj.btn_meter1, 1, n);
  styleMeterBtn(settingsObj.btn_meter2, 2, n);
  styleMeterBtn(settingsObj.btn_meter3, 3, n);
}

void updateBlePanelLabels() {
  char roomMac[H2_MAC_STR_LEN];
  char outMac[H2_MAC_STR_LEN];
  climateRoomGetConfiguredMac(roomMac, sizeof(roomMac));
  climateRoomGetConfiguredOutdoorMac(outMac, sizeof(outMac));

  char line[96];
  const ClimateBridgeOtaState otaSt = climateRoomBridgeOtaState();
  if (otaSt == CLIMATE_BRIDGE_OTA_CONNECTING) {
    snprintf(line, sizeof(line), "Bridge OTA: připojuji Wi-Fi...");
  } else if (otaSt == CLIMATE_BRIDGE_OTA_READY) {
    snprintf(line, sizeof(line), "Bridge OTA: %s (%s)",
             climateRoomBridgeOtaHost(), climateRoomBridgeOtaIp());
  } else if (otaSt == CLIMATE_BRIDGE_OTA_FAIL) {
    snprintf(line, sizeof(line), "Bridge OTA selhalo - zkontroluj Wi-Fi Tab5");
  } else if (climateRoomIsBusy()) {
    snprintf(line, sizeof(line), "Skenuji SwitchBot...");
  } else {
    const int n = climateRoomFoundCount();
    // Po skenu vždy nabídni výběr (i když už máme teplotu z dřívějšího MAC)
    if (n > 0) {
      snprintf(line, sizeof(line), "Nalezeno %d - vyber Tepl.1-%d", n,
               n > 3 ? 3 : n);
    } else if (climateRoomIsOk() && climateRoomOutdoorIsOk()) {
      const int outBat = climateRoomOutdoorBatteryPct();
      if (outBat >= 0) {
        snprintf(line, sizeof(line), "Pokoj %.1f °C · Venku %.1f °C (bat %d%%)",
                 (double)climateRoomTempC(), (double)climateRoomOutdoorTempC(),
                 outBat);
      } else {
        snprintf(line, sizeof(line), "Pokoj %.1f °C · Venku %.1f °C",
                 (double)climateRoomTempC(), (double)climateRoomOutdoorTempC());
      }
    } else if (climateRoomIsOk()) {
      snprintf(line, sizeof(line), "Pokoj %.1f °C", (double)climateRoomTempC());
    } else if (climateRoomOutdoorIsOk()) {
      const int outBat = climateRoomOutdoorBatteryPct();
      const int outRssi = climateRoomOutdoorRssi();
      if (outBat >= 0) {
        snprintf(line, sizeof(line), "Venku %.1f °C · bat %d%% · rssi %d",
                 (double)climateRoomOutdoorTempC(), outBat, outRssi);
      } else {
        snprintf(line, sizeof(line), "Venku %.1f °C", (double)climateRoomOutdoorTempC());
      }
    } else {
      snprintf(line, sizeof(line), "Čekám na data - Skenuj / MAC");
    }
  }
  setLabelIfChanged(settingsObj.lbl_sys_hint, line);

  char macLine[48];
  snprintf(macLine, sizeof(macLine), "Pokoj: %s", roomMac);
  setLabelIfChanged(settingsObj.lbl_mac_room, macLine);
  snprintf(macLine, sizeof(macLine), "Venku: %s", outMac);
  setLabelIfChanged(settingsObj.lbl_mac_out, macLine);

  char rsp[96];
  climateRoomGetLastRoomResponse(rsp, sizeof(rsp));
  setLabelIfChanged(settingsObj.lbl_rsp_room, rsp);
  setTextColorCached(settingsObj.lbl_rsp_room,
                     climateRoomIsOk() ? kColText : kColMuted);

  climateRoomGetLastOutdoorResponse(rsp, sizeof(rsp));
  setLabelIfChanged(settingsObj.lbl_rsp_out, rsp);
  setTextColorCached(settingsObj.lbl_rsp_out,
                     climateRoomOutdoorIsOk() ? kColText : kColMuted);

  uint32_t col = kColMuted;
  if (otaSt == CLIMATE_BRIDGE_OTA_READY) {
    col = kColGreen;
  } else if (otaSt == CLIMATE_BRIDGE_OTA_CONNECTING) {
    col = kColOrange;
  } else if (otaSt == CLIMATE_BRIDGE_OTA_FAIL) {
    col = kColOrange;
  } else if (climateRoomIsOk() || climateRoomOutdoorIsOk()) {
    col = kColGreen;
  } else if (climateRoomIsBusy()) {
    col = kColOrange;
  }
  setTextColorCached(settingsObj.lbl_sys_hint, col);
  updateMeterButtons();
}

void updateSleepLabels() {
  if (settingsObj.btn_sleep) {
    lv_obj_t* lbl = lv_obj_get_child(settingsObj.btn_sleep, 0);
    if (lbl) {
      char line[32];
      snprintf(line, sizeof(line), "Usínání: %s", uiDisplaySleepTimeoutLabel());
      setLabelIfChanged(lbl, line);
    }
  }
}

void onBrightnessChanged(lv_event_t* e) {
  lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
  const int32_t v = lv_slider_get_value(slider);
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    uiDisplaySetBrightness(static_cast<uint8_t>(v), false);
    updateBrightnessLabel();
  } else if (code == LV_EVENT_RELEASED) {
    uiDisplaySetBrightness(static_cast<uint8_t>(v), true);
    updateBrightnessLabel();
  }
}

void onSleepCycle(lv_event_t* e) {
  (void)e;
  uiDisplayCycleSleepTimeout();
  updateSleepLabels();
}

}  // namespace

void uiSettingsCreate() {
  memset(&settingsObj, 0, sizeof(settingsObj));

  lv_obj_t* scr = lv_obj_create(nullptr);
  settingsObj.screen = scr;
  lv_obj_set_size(scr, kW, kH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  const int contentW = kW - 2 * kMargin;
  const int colW = (contentW - kGap) / 2;
  const int toggleW = 130;
  const int btnW = (colW - 2 * kPad - kGap) / 2;
  const int labelW = colW - 2 * kPad;
  const int titleW = colW - toggleW - 3 * kPad;

  settingsObj.btn_back = makeButton(
      scr, kMargin, 4, 120, kBtnH, "<- ZPĚT", action_akce_zpet, 0x48484Fu);
  settingsObj.lbl_title = makeLabel(scr, 0, 10, 0, "NASTAVENÍ", kColText);
  lv_obj_set_style_text_font(settingsObj.lbl_title, kFontTitle, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_align(settingsObj.lbl_title, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
  char versionText[24];
  snprintf(versionText, sizeof(versionText), "%s", APP_FW_VERSION);
  settingsObj.lbl_version = makeLabel(scr, 0, 10, 220, versionText, kColMuted);
  lv_obj_set_style_align(settingsObj.lbl_version, LV_ALIGN_TOP_RIGHT,
                         LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_x(settingsObj.lbl_version, -kMargin);

  const int topY = kHeaderH;
  const int topH = 220;
  settingsObj.panel_wifi = makePanel(scr, kMargin, topY, colW, topH);
  settingsObj.lbl_wifi_title =
      makeLabel(settingsObj.panel_wifi, kPad, kTitleY, titleW, "Wi-Fi", kColOrange);
  settingsObj.lbl_wifi_status =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY, labelW, "Stav: ---", kColText);
  settingsObj.lbl_wifi_ssid =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY + kLine, labelW, "Síť: ---", kColMuted);
  settingsObj.lbl_wifi_ip =
      makeLabel(settingsObj.panel_wifi, kPad, kBodyY + 2 * kLine, labelW, "IP: ---", kColMuted);
  settingsObj.btn_wifi_toggle = makeButton(
      settingsObj.panel_wifi, colW - toggleW - kPad, kTitleY, toggleW, kBtnH,
      "Vypnuto", action_akce_wifi_toggle, kColAccent);
  const int wifiBtnY = topH - kBtnH - kPad;
  settingsObj.btn_wifi_connect = makeButton(
      settingsObj.panel_wifi, kPad, wifiBtnY, btnW, kBtnH,
      "Připojit", action_akce_wifi_connect, kColAccent);
  settingsObj.btn_wifi_edit = makeButton(
      settingsObj.panel_wifi, kPad + btnW + kGap, wifiBtnY, btnW, kBtnH,
      "Nastavit síť", action_akce_wifi_edit, kColPurple);

  settingsObj.panel_mqtt = makePanel(scr, kMargin + colW + kGap, topY, colW, topH);
  settingsObj.lbl_mqtt_title =
      makeLabel(settingsObj.panel_mqtt, kPad, kTitleY, titleW, "MQTT", kColOrange);
  settingsObj.lbl_mqtt_status =
      makeLabel(settingsObj.panel_mqtt, kPad, kBodyY, labelW, "Stav: ---", kColText);
  settingsObj.lbl_mqtt_host =
      makeLabel(settingsObj.panel_mqtt, kPad, kBodyY + kLine, labelW, "Broker: ---", kColMuted);
  settingsObj.btn_mqtt_toggle = makeButton(
      settingsObj.panel_mqtt, colW - toggleW - kPad, kTitleY, toggleW, kBtnH,
      "Vypnuto", action_akce_mqtt_toggle, kColAccent);
  settingsObj.btn_mqtt_connect = makeButton(
      settingsObj.panel_mqtt, kPad, topH - kBtnH - kPad, 180, kBtnH,
      "Připojit MQTT", action_akce_mqtt_connect, kColAccent);

  const int dispY = topY + topH + kGap;
  const int dispH = 58;
  settingsObj.panel_display = makePanel(scr, kMargin, dispY, contentW, dispH);
  settingsObj.lbl_disp_title =
      makeLabel(settingsObj.panel_display, kPad, 16, 80, "Displej", kColOrange);
  settingsObj.lbl_brightness =
      makeLabel(settingsObj.panel_display, kPad + 90, 16, 110, "Jas 60%", kColText);

  const int sliderX = kPad + 200;
  const int sleepBtnW = 190;
  const int sliderW = contentW - sliderX - sleepBtnW - 2 * kPad - kGap;
  settingsObj.slider_brightness = lv_slider_create(settingsObj.panel_display);
  lv_obj_set_pos(settingsObj.slider_brightness, sliderX, 18);
  lv_obj_set_size(settingsObj.slider_brightness, sliderW, 24);
  lv_slider_set_range(settingsObj.slider_brightness, 10, 97);
  lv_slider_set_value(settingsObj.slider_brightness, uiDisplayGetBrightness(), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(0x3A3A3Cu),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(kColAccent),
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(settingsObj.slider_brightness, lv_color_hex(0xFFFFFFu),
                            LV_PART_KNOB | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(settingsObj.slider_brightness, onBrightnessChanged,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(settingsObj.slider_brightness, onBrightnessChanged, LV_EVENT_RELEASED,
                      nullptr);

  settingsObj.lbl_sleep = nullptr;
  settingsObj.btn_sleep = makeButton(
      settingsObj.panel_display, contentW - kPad - sleepBtnW, 10, sleepBtnW, kBtnH,
      "Usínání: 2 min", onSleepCycle, kColPurple);

  const int sysY = dispY + dispH + kGap;
  const int sysH = kH - sysY - kMargin;
  settingsObj.panel_sys = makePanel(scr, kMargin, sysY, contentW, sysH);
  lv_obj_set_style_bg_opa(settingsObj.panel_sys, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(settingsObj.panel_sys, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  const int innerW = contentW - 2 * kPad;
  const int thirdW = (innerW - 2 * kGap) / 3;
  // SwitchBot panel = 1/3 + kus nav prostoru (širší kvůli MAC řádkům)
  const int bleW = thirdW + (thirdW / 4);
  const int navColW = innerW - bleW - kGap;
  const int navColX = kPad + bleW + kGap;

  const int bleH = sysH - 2 * kPad;
  settingsObj.panel_ble = makePanel(settingsObj.panel_sys, kPad, kPad, bleW, bleH);

  settingsObj.lbl_sys_title =
      makeLabel(settingsObj.panel_ble, kPad, kTitleY, bleW - 2 * kPad, "SwitchBot", kColOrange);
  settingsObj.lbl_sys_hint = makeLabel(settingsObj.panel_ble, kPad, kBodyY,
                                       bleW - 2 * kPad, "Čekám na H2...", kColMuted);
  lv_label_set_long_mode(settingsObj.lbl_sys_hint, LV_LABEL_LONG_WRAP);

  settingsObj.lbl_mac_room =
      makeLabel(settingsObj.panel_ble, kPad, kBodyY + kLine, bleW - 2 * kPad,
                "Pokoj: ---", kColMuted);
  settingsObj.lbl_mac_out =
      makeLabel(settingsObj.panel_ble, kPad, kBodyY + 2 * kLine, bleW - 2 * kPad,
                "Venku: ---", kColMuted);
  settingsObj.lbl_rsp_room =
      makeLabel(settingsObj.panel_ble, kPad, kBodyY + 3 * kLine, bleW - 2 * kPad,
                "---", kColMuted);
  lv_label_set_long_mode(settingsObj.lbl_rsp_room, LV_LABEL_LONG_WRAP);
  settingsObj.lbl_rsp_out =
      makeLabel(settingsObj.panel_ble, kPad, kBodyY + 4 * kLine, bleW - 2 * kPad,
                "---", kColMuted);
  lv_label_set_long_mode(settingsObj.lbl_rsp_out, LV_LABEL_LONG_WRAP);

  const int row2Y = bleH - kBtnH - kPad;
  const int row1Y = row2Y - kBtnH - kGap;
  const int btnThirdW = (bleW - 2 * kPad - 2 * kGap) / 3;
  const int pickW = (bleW - 2 * kPad - 2 * kGap) / 3;

  settingsObj.btn_ble = makeButton(
      settingsObj.panel_ble, kPad, row1Y, btnThirdW, kBtnH,
      "Skenuj", action_akce_settings_ble, kColPurple);
  settingsObj.btn_mac = makeButton(
      settingsObj.panel_ble, kPad + btnThirdW + kGap, row1Y, btnThirdW, kBtnH,
      "MAC", action_akce_settings_ble_mac, kColAccent);
  settingsObj.btn_bridge_ota = makeButton(
      settingsObj.panel_ble, kPad + 2 * (btnThirdW + kGap), row1Y, btnThirdW,
      kBtnH, "OTA", action_akce_settings_bridge_ota, kColGreen);
  settingsObj.btn_meter1 = makeButton(
      settingsObj.panel_ble, kPad, row2Y, pickW, kBtnH,
      "Tepl.1", action_akce_settings_meter1, kColAccent);
  settingsObj.btn_meter2 = makeButton(
      settingsObj.panel_ble, kPad + pickW + kGap, row2Y, pickW, kBtnH,
      "Tepl.2", action_akce_settings_meter2, kColAccent);
  settingsObj.btn_meter3 = makeButton(
      settingsObj.panel_ble, kPad + 2 * (pickW + kGap), row2Y, pickW, kBtnH,
      "Tepl.3", action_akce_settings_meter3, kColAccent);
  lv_obj_add_flag(settingsObj.btn_meter1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(settingsObj.btn_meter2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(settingsObj.btn_meter3, LV_OBJ_FLAG_HIDDEN);

  constexpr int kNavBtnH = 52;
  const int navBtnW = (navColW - kGap) / 2;
  const int navY = sysH - kPad - kNavBtnH;
  settingsObj.btn_plan = makeButton(
      settingsObj.panel_sys, navColX, navY, navBtnW, kNavBtnH,
      "Plan", action_akce_settings_plan, kColPurple);
  settingsObj.btn_servis = makeButton(
      settingsObj.panel_sys, navColX + navBtnW + kGap, navY, navBtnW, kNavBtnH,
      "Regulátor", action_akce_settings_servis, kColPurple);

  strncpy(uiEez.set_sys_hint, "Čekám na H2...", sizeof(uiEez.set_sys_hint) - 1);
  uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';

  updateBrightnessLabel();
  updateSleepLabels();
  updateBlePanelLabels();
  uiSettingsTick();
}

void uiSettingsTick() {
  if (!settingsObj.screen) {
    return;
  }

  char line[80];

  snprintf(line, sizeof(line), "Stav: %s", uiEez.set_wifi_status);
  setLabelIfChanged(settingsObj.lbl_wifi_status, line);
  setTextColorCached(settingsObj.lbl_wifi_status, get_var_sig_wifi_color());

  snprintf(line, sizeof(line), "Síť: %s", uiEez.set_wifi_ssid);
  setLabelIfChanged(settingsObj.lbl_wifi_ssid, line);

  snprintf(line, sizeof(line), "IP: %s", uiEez.set_wifi_ip);
  setLabelIfChanged(settingsObj.lbl_wifi_ip, line);

  snprintf(line, sizeof(line), "Stav: %s", uiEez.set_mqtt_status);
  setLabelIfChanged(settingsObj.lbl_mqtt_status, line);
  setTextColorCached(settingsObj.lbl_mqtt_status, get_var_sig_mqtt_color());

  snprintf(line, sizeof(line), "Broker: %s", uiEez.set_mqtt_host);
  setLabelIfChanged(settingsObj.lbl_mqtt_host, line);

  styleToggleBtn(settingsObj.btn_wifi_toggle, uiEez.set_wifi_enabled);
  styleToggleBtn(settingsObj.btn_mqtt_toggle, uiEez.set_mqtt_enabled);

  const bool mqttReady = uiEez.set_wifi_enabled && uiEez.sig_wifi;
  styleConnectBtn(settingsObj.btn_mqtt_connect, mqttReady, uiEez.sig_mqtt);
  updateBlePanelLabels();

  // Sync z mgr (po NVS load / změně mimo LVGL eventy)
  if (settingsObj.slider_brightness) {
    const uint8_t b = uiDisplayGetBrightness();
    if ((uint8_t)lv_slider_get_value(settingsObj.slider_brightness) != b) {
      lv_slider_set_value(settingsObj.slider_brightness, b, LV_ANIM_OFF);
    }
  }
  updateBrightnessLabel();
  updateSleepLabels();
}

void uiSettingsShowBridgeOtaHint(const char* hint) {
  if (!hint) {
    return;
  }
  strncpy(uiEez.set_sys_hint, hint, sizeof(uiEez.set_sys_hint) - 1);
  uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
  if (settingsObj.lbl_sys_hint) {
    setLabelIfChanged(settingsObj.lbl_sys_hint, hint);
    setTextColorCached(settingsObj.lbl_sys_hint, kColOrange);
  }
}

void uiSettingsShowScanning() {
  if (!settingsObj.lbl_sys_hint) {
    return;
  }
  setLabelIfChanged(settingsObj.lbl_sys_hint, "Skenuji SwitchBot...");
  setTextColorCached(settingsObj.lbl_sys_hint, kColOrange);
}

void uiSettingsSetBrightnessFromTouch(uint8_t percent, bool persist) {
  if (settingsObj.slider_brightness) {
    lv_slider_set_value(settingsObj.slider_brightness, percent, LV_ANIM_OFF);
  }
  uiDisplaySetBrightness(percent, persist);
  updateBrightnessLabel();
}

lv_obj_t* uiSettingsScreen() {
  return settingsObj.screen;
}
