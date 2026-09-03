// climate_room_uart.cpp — UART z ESP32-H2 (SwitchBot bridge)
#include "climate_room_uart.h"

#include "ble_config.h"
#include "climate_energy.h"
#include "net_wifi_mgr.h"
#include "storage_config_nvs.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

HardwareSerial RoomSerial(2);

constexpr uint32_t kStaleMs = 15UL * 60UL * 1000UL;
constexpr size_t kLineMax = 96;

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
bool s_inited = false;
bool s_ok = false;
bool s_outOk = false;
bool s_scanBusy = false;
uint32_t s_scanStartedMs = 0;
constexpr uint32_t kScanTimeoutMs = 15000;
float s_tempC = UI_TEPLOTA_NEPLATNA;
float s_hum = UI_TEPLOTA_NEPLATNA;
int s_batt = -1;
int s_rssi = 0;
uint32_t s_lastSeenMs = 0;

float s_outTempC = UI_TEPLOTA_NEPLATNA;
float s_outHum = UI_TEPLOTA_NEPLATNA;
int s_outBatt = -1;
int s_outRssi = 0;
uint32_t s_outLastSeenMs = 0;

ClimateRoomFound s_found[H2_FOUND_MAX];
int s_foundCount = 0;

char s_cfgMac[H2_MAC_STR_LEN] = "";
char s_cfgOutMac[H2_MAC_STR_LEN] = "";
bool s_cfgMacLoaded = false;

char s_line[kLineMax];
size_t s_lineLen = 0;
uint32_t s_macResyncAtMs = 0;
char s_lastRoomRsp[kLineMax] = "";
char s_lastOutRsp[kLineMax] = "";

ClimateBridgeOtaState s_bridgeOtaState = CLIMATE_BRIDGE_OTA_IDLE;
char s_bridgeOtaIp[20] = "";
char s_bridgeOtaHost[40] = "";
uint32_t s_bridgeOtaFailAtMs = 0;

char s_bridgeMac[H2_MAC_STR_LEN] = "";
uint8_t s_bridgeCh = 0;
bool s_bridgeEspNow = false;
uint32_t s_bridgeInfoAtMs = 0;

uint16_t s_lastPwrW = 0;
float s_lastPwrKwh = 0.0f;
uint32_t s_lastPwrAtMs = 0;

bool isZeroMac(const char* mac) {
  return !mac || mac[0] == '\0' || strcmp(mac, "00:00:00:00:00:00") == 0 ||
         strcmp(mac, "---") == 0 || strcmp(mac, "—") == 0;
}

bool looksLikeMac(const char* mac) {
  return mac && strlen(mac) == 17 && mac[2] == ':' && mac[5] == ':';
}

void refreshCfgMacCache() {
  char mac[H2_MAC_STR_LEN];
  if (storageLoadBleRoomMac(mac, sizeof(mac))) {
    strncpy(s_cfgMac, mac, sizeof(s_cfgMac) - 1);
    s_cfgMac[sizeof(s_cfgMac) - 1] = '\0';
  } else {
    strncpy(s_cfgMac, BLE_METER_MAC, sizeof(s_cfgMac) - 1);
    s_cfgMac[sizeof(s_cfgMac) - 1] = '\0';
  }
  if (storageLoadBleOutdoorMac(mac, sizeof(mac))) {
    strncpy(s_cfgOutMac, mac, sizeof(s_cfgOutMac) - 1);
    s_cfgOutMac[sizeof(s_cfgOutMac) - 1] = '\0';
  } else {
    strncpy(s_cfgOutMac, BLE_OUTDOOR_MAC, sizeof(s_cfgOutMac) - 1);
    s_cfgOutMac[sizeof(s_cfgOutMac) - 1] = '\0';
  }
  s_cfgMacLoaded = true;
}

void sendCmd(const char* cmd) {
  if (!cmd) {
    return;
  }
  RoomSerial.print(cmd);
  RoomSerial.print('\n');
  Serial.printf("[ROOM] -> %s\n", cmd);
}

bool humValid(float h) {
  return h >= 0.0f && h <= 100.0f && h > (UI_TEPLOTA_NEPLATNA + 1.0f);
}

void formatSensorLineUnlocked(char* buf, size_t len, bool outdoor) {
  if (!buf || len == 0) {
    return;
  }
  float t = UI_TEPLOTA_NEPLATNA;
  float h = UI_TEPLOTA_NEPLATNA;
  int batt = -1;
  int rssi = 0;
  bool ok = false;
  if (outdoor) {
    ok = s_outOk;
    t = s_outTempC;
    h = s_outHum;
    batt = s_outBatt;
    rssi = s_outRssi;
  } else {
    ok = s_ok;
    t = s_tempC;
    h = s_hum;
    batt = s_batt;
    rssi = s_rssi;
  }

  if (!ok || t <= (UI_TEPLOTA_NEPLATNA + 1.0f)) {
    strncpy(buf, "---", len - 1);
    buf[len - 1] = '\0';
    return;
  }

  size_t n = 0;
  if (outdoor) {
    n = snprintf(buf, len, "OUT T=%.1f", t);
  } else {
    n = snprintf(buf, len, "T=%.1f", t);
  }
  if (humValid(h)) {
    n += snprintf(buf + n, len - n, " H=%.0f", h);
  }
  if (batt >= 0) {
    n += snprintf(buf + n, len - n, " B=%d", batt);
  }
  snprintf(buf + n, len - n, " R=%d", rssi);
}

void formatSensorLine(char* buf, size_t len, bool outdoor) {
  portENTER_CRITICAL(&s_mux);
  formatSensorLineUnlocked(buf, len, outdoor);
  portEXIT_CRITICAL(&s_mux);
}

void applyRoomReading(float t, float h, int batt, int rssi) {
  portENTER_CRITICAL(&s_mux);
  s_tempC = t;
  s_rssi = rssi;
  if (humValid(h)) {
    s_hum = h;
  }
  if (batt >= 0) {
    s_batt = batt;
  }
  s_ok = true;
  s_lastSeenMs = millis();
  formatSensorLineUnlocked(s_lastRoomRsp, sizeof(s_lastRoomRsp), false);
  portEXIT_CRITICAL(&s_mux);

  uiEez.sig_ble = true;
  uiEez.teplota_vnitrni = t;
  Serial.printf("[ROOM] H2 T=%.1f H=%.1f bat=%d rssi=%d\n", t, h, batt, rssi);
}

void applyOutdoorReading(float t, float h, int batt, int rssi) {
  portENTER_CRITICAL(&s_mux);
  s_outTempC = t;
  s_outRssi = rssi;
  if (humValid(h)) {
    s_outHum = h;
  }
  if (batt >= 0) {
    s_outBatt = batt;
  }
  s_outOk = true;
  s_outLastSeenMs = millis();
  formatSensorLineUnlocked(s_lastOutRsp, sizeof(s_lastOutRsp), true);
  portEXIT_CRITICAL(&s_mux);

  uiEez.teplota_venkovni = t;
  Serial.printf("[ROOM] H2 OUT T=%.1f H=%.1f bat=%d rssi=%d\n", t, h, batt, rssi);
  s_macResyncAtMs = 0;
}

void handleFoundLine(const char* line) {
  int idx = 0;
  char mac[H2_MAC_STR_LEN];
  float temp = UI_TEPLOTA_NEPLATNA;
  int rssi = 0;
  if (sscanf(line, "FOUND %d %17s T=%f R=%d", &idx, mac, &temp, &rssi) < 3) {
    return;
  }
  if (idx < 1 || idx > H2_FOUND_MAX) {
    return;
  }
  portENTER_CRITICAL(&s_mux);
  ClimateRoomFound* e = &s_found[idx - 1];
  strncpy(e->mac, mac, sizeof(e->mac) - 1);
  e->mac[sizeof(e->mac) - 1] = '\0';
  e->temp_c = temp;
  e->rssi = rssi;
  e->valid = true;
  if (idx > s_foundCount) {
    s_foundCount = idx;
  }
  portEXIT_CRITICAL(&s_mux);
}

void copyMacToken(const char* src, char* dst, size_t dstLen) {
  size_t i = 0;
  while (src[i] && src[i] != ' ' && i + 1 < dstLen) {
    dst[i] = src[i];
    ++i;
  }
  dst[i] = '\0';
}

void handleCfgLine(const char* line) {
  // CFG ROOM=... OUT=...
  const char* room = strstr(line, "ROOM=");
  const char* out = strstr(line, "OUT=");
  char mac[H2_MAC_STR_LEN];
  Serial.printf("[ROOM] H2 %s\n", line);
  if (room) {
    copyMacToken(room + 5, mac, sizeof(mac));
    if (!isZeroMac(mac) && looksLikeMac(mac)) {
      storageSaveBleRoomMac(mac);
      strncpy(s_cfgMac, mac, sizeof(s_cfgMac) - 1);
      s_cfgMac[sizeof(s_cfgMac) - 1] = '\0';
      s_cfgMacLoaded = true;
    }
  }
  if (out) {
    copyMacToken(out + 4, mac, sizeof(mac));
    if (!isZeroMac(mac) && looksLikeMac(mac)) {
      storageSaveBleOutdoorMac(mac);
      strncpy(s_cfgOutMac, mac, sizeof(s_cfgOutMac) - 1);
      s_cfgOutMac[sizeof(s_cfgOutMac) - 1] = '\0';
      s_cfgMacLoaded = true;
    }
  }
}

void parseTelemetryFields(char* line, float* t, float* h, int* batt, int* rssi,
                          bool* haveT) {
  *t = UI_TEPLOTA_NEPLATNA;
  *h = UI_TEPLOTA_NEPLATNA;
  *batt = -1;
  *rssi = 0;
  *haveT = false;
  char* tok = strtok(line, " \t");
  while (tok) {
    if (tok[0] == 'T' && tok[1] == '=') {
      *t = strtof(tok + 2, nullptr);
      *haveT = (*t > -40.0f && *t < 85.0f);
    } else if (tok[0] == 'H' && tok[1] == '=') {
      *h = strtof(tok + 2, nullptr);
    } else if (tok[0] == 'B' && tok[1] == '=') {
      *batt = atoi(tok + 2);
    } else if (tok[0] == 'R' && tok[1] == '=') {
      *rssi = atoi(tok + 2);
    }
    tok = strtok(nullptr, " \t");
  }
}

void handleBridgeInfoLine(const char* line) {
  if (!line || strncmp(line, H2_PREFIX_INFO, strlen(H2_PREFIX_INFO)) != 0) {
    return;
  }
  const char* p = line + strlen(H2_PREFIX_INFO);
  char mac[H2_MAC_STR_LEN] = "";
  unsigned ch = 0;
  unsigned espnow = 0;
  // INFO MAC=AA:BB:CC:DD:EE:FF CH=1 ESPNOW=1
  const char* macKey = strstr(p, "MAC=");
  const char* chKey = strstr(p, "CH=");
  const char* enKey = strstr(p, "ESPNOW=");
  if (macKey) {
    macKey += 4;
    size_t i = 0;
    while (macKey[i] && macKey[i] != ' ' && i + 1 < sizeof(mac)) {
      mac[i] = macKey[i];
      ++i;
    }
    mac[i] = '\0';
  }
  if (chKey) {
    ch = (unsigned)atoi(chKey + 3);
  }
  if (enKey) {
    espnow = (unsigned)atoi(enKey + 7);
  }
  portENTER_CRITICAL(&s_mux);
  if (mac[0]) {
    strncpy(s_bridgeMac, mac, sizeof(s_bridgeMac) - 1);
    s_bridgeMac[sizeof(s_bridgeMac) - 1] = '\0';
  }
  s_bridgeCh = (uint8_t)ch;
  s_bridgeEspNow = (espnow != 0);
  s_bridgeInfoAtMs = millis();
  portEXIT_CRITICAL(&s_mux);
  Serial.printf("[ROOM] bridge INFO mac=%s ch=%u espnow=%u\n", s_bridgeMac,
                (unsigned)s_bridgeCh, s_bridgeEspNow ? 1u : 0u);
}

void handleBridgeWifiLine(const char* line) {
  if (!line) {
    return;
  }
  if (strncmp(line, H2_PREFIX_WIFI, strlen(H2_PREFIX_WIFI)) == 0) {
    const char* p = line + strlen(H2_PREFIX_WIFI);
    if (strcmp(p, "CONNECTING") == 0) {
      s_bridgeOtaState = CLIMATE_BRIDGE_OTA_CONNECTING;
      s_bridgeOtaIp[0] = '\0';
      s_bridgeOtaHost[0] = '\0';
      Serial.println("[ROOM] bridge WiFi connecting");
      return;
    }
    if (strncmp(p, "IP=", 3) == 0) {
      strncpy(s_bridgeOtaIp, p + 3, sizeof(s_bridgeOtaIp) - 1);
      s_bridgeOtaIp[sizeof(s_bridgeOtaIp) - 1] = '\0';
      Serial.printf("[ROOM] bridge IP %s\n", s_bridgeOtaIp);
      return;
    }
    if (strcmp(p, "OFF") == 0) {
      s_bridgeOtaState = CLIMATE_BRIDGE_OTA_IDLE;
      s_bridgeOtaIp[0] = '\0';
      s_bridgeOtaHost[0] = '\0';
      Serial.println("[ROOM] bridge WiFi off");
      return;
    }
  }
  if (strncmp(line, H2_PREFIX_OTA, strlen(H2_PREFIX_OTA)) == 0) {
    const char* p = line + strlen(H2_PREFIX_OTA);
    if (strncmp(p, "READY ", 6) == 0) {
      s_bridgeOtaState = CLIMATE_BRIDGE_OTA_READY;
      const char* host = strstr(p + 6, "host=");
      const char* ip = strstr(p + 6, "ip=");
      if (host) {
        host += 5;
        size_t i = 0;
        while (host[i] && host[i] != ' ' && i + 1 < sizeof(s_bridgeOtaHost)) {
          s_bridgeOtaHost[i] = host[i];
          ++i;
        }
        s_bridgeOtaHost[i] = '\0';
      }
      if (ip) {
        ip += 3;
        size_t i = 0;
        while (ip[i] && ip[i] != ' ' && i + 1 < sizeof(s_bridgeOtaIp)) {
          s_bridgeOtaIp[i] = ip[i];
          ++i;
        }
        s_bridgeOtaIp[i] = '\0';
      }
      Serial.printf("[ROOM] bridge OTA ready %s %s\n", s_bridgeOtaHost,
                    s_bridgeOtaIp);
      return;
    }
    if (strncmp(p, "DONE", 4) == 0) {
      s_bridgeOtaState = CLIMATE_BRIDGE_OTA_IDLE;
      Serial.println("[ROOM] bridge OTA done — reboot");
      return;
    }
  }
  if (strncmp(line, H2_PREFIX_ERR, strlen(H2_PREFIX_ERR)) == 0) {
    const char* p = line + strlen(H2_PREFIX_ERR);
    if (strncmp(p, "WIFI", 4) == 0) {
      s_bridgeOtaState = CLIMATE_BRIDGE_OTA_FAIL;
      s_bridgeOtaFailAtMs = millis() + 8000UL;
      Serial.println("[ROOM] bridge WiFi fail");
    }
  }
}

void parseLine(char* line) {
  if (!line || !line[0]) {
    return;
  }

  if (strncmp(line, H2_PREFIX_PWR, strlen(H2_PREFIX_PWR)) == 0) {
    unsigned w = 0;
    float e = 0.0f;
    unsigned r = 0;
    if (sscanf(line + strlen(H2_PREFIX_PWR), "W=%u E=%f R=%u", &w, &e, &r) >=
        2) {
      portENTER_CRITICAL(&s_mux);
      s_lastPwrW = (uint16_t)w;
      s_lastPwrKwh = e;
      s_lastPwrAtMs = millis();
      portEXIT_CRITICAL(&s_mux);
      climateEnergyOnSample((uint16_t)w, e, r != 0);
    }
    return;
  }
  if (strncmp(line, H2_PREFIX_INFO, strlen(H2_PREFIX_INFO)) == 0) {
    handleBridgeInfoLine(line);
    return;
  }
  if (strncmp(line, H2_PREFIX_FOUND, strlen(H2_PREFIX_FOUND)) == 0) {
    handleFoundLine(line);
    return;
  }
  if (strncmp(line, H2_PREFIX_SCAN_DONE, strlen(H2_PREFIX_SCAN_DONE)) == 0) {
    s_scanBusy = false;
    s_scanStartedMs = 0;
    Serial.printf("[ROOM] %s\n", line);
    return;
  }
  if (strncmp(line, H2_PREFIX_CFG, strlen(H2_PREFIX_CFG)) == 0) {
    handleCfgLine(line);
    return;
  }
  if (strncmp(line, H2_PREFIX_WIFI, strlen(H2_PREFIX_WIFI)) == 0 ||
      strncmp(line, H2_PREFIX_OTA, strlen(H2_PREFIX_OTA)) == 0) {
    handleBridgeWifiLine(line);
    return;
  }
  if (strncmp(line, H2_PREFIX_ERR, strlen(H2_PREFIX_ERR)) == 0) {
    handleBridgeWifiLine(line);
    return;
  }

  const bool isOut = (strncmp(line, "OUT ", 4) == 0);
  char* payload = isOut ? line + 4 : line;

  float t = UI_TEPLOTA_NEPLATNA;
  float h = UI_TEPLOTA_NEPLATNA;
  int batt = -1;
  int rssi = 0;
  bool haveT = false;
  parseTelemetryFields(payload, &t, &h, &batt, &rssi, &haveT);
  if (!haveT) {
    return;
  }
  if (isOut) {
    applyOutdoorReading(t, h, batt, rssi);
  } else {
    applyRoomReading(t, h, batt, rssi);
  }
}

void feedByte(char c) {
  if (c == '\r') {
    return;
  }
  if (c == '\n') {
    if (s_lineLen > 0) {
      s_line[s_lineLen] = '\0';
      parseLine(s_line);
      s_lineLen = 0;
    }
    return;
  }
  if (s_lineLen + 1 < kLineMax) {
    s_line[s_lineLen++] = c;
  } else {
    s_lineLen = 0;
  }
}

void pushStoredMacToH2() {
  if (!s_cfgMacLoaded) {
    refreshCfgMacCache();
  }
  if (!isZeroMac(s_cfgMac) && looksLikeMac(s_cfgMac)) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "SET ROOM=%s", s_cfgMac);
    sendCmd(cmd);
  }
  if (!isZeroMac(s_cfgOutMac) && looksLikeMac(s_cfgOutMac)) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "SET OUT=%s", s_cfgOutMac);
    sendCmd(cmd);
  }
  if (isZeroMac(s_cfgMac) && isZeroMac(s_cfgOutMac)) {
    sendCmd("GET CFG");
  } else {
    // Hned požádej H2 o první reading (nečekat na interval pollu)
    sendCmd(H2_CMD_POLL);
  }
}

}  // namespace

void climateRoomInit(void) {
  if (s_inited) {
    return;
  }
  RoomSerial.begin(CLIMATE_ROOM_UART_BAUD, SERIAL_8N1,
                   CLIMATE_ROOM_UART_RX_PIN, CLIMATE_ROOM_UART_TX_PIN);
  s_inited = true;
  s_lineLen = 0;
  refreshCfgMacCache();
  Serial.printf("[ROOM] UART H2 RX=G%d TX=G%d @ %u\n",
                CLIMATE_ROOM_UART_RX_PIN, CLIMATE_ROOM_UART_TX_PIN,
                (unsigned)CLIMATE_ROOM_UART_BAUD);
  Serial.printf("[ROOM] cfg room=%s out=%s\n", s_cfgMac, s_cfgOutMac);
  delay(500);
  sendCmd("GET CFG");
  delay(50);
  sendCmd(H2_CMD_GET_INFO);
  delay(100);
  pushStoredMacToH2();
  s_macResyncAtMs = millis() + 60000UL;
}

void climateRoomTick(void) {
  if (!s_inited) {
    climateRoomInit();
  }

  while (RoomSerial.available() > 0) {
    feedByte((char)RoomSerial.read());
  }

  if (s_scanBusy && s_scanStartedMs != 0 &&
      (millis() - s_scanStartedMs) > kScanTimeoutMs) {
    s_scanBusy = false;
    s_scanStartedMs = 0;
    Serial.println("[ROOM] scan timeout");
  }

  if (!climateRoomOutdoorIsOk() && s_macResyncAtMs != 0 &&
      millis() >= s_macResyncAtMs) {
    s_macResyncAtMs = millis() + 60000UL;
    Serial.println("[ROOM] outdoor stale — resync MAC + POLL");
    pushStoredMacToH2();
  }

  bool ok = false;
  bool outOk = false;
  float temp = UI_TEPLOTA_NEPLATNA;
  float outTemp = UI_TEPLOTA_NEPLATNA;
  uint32_t lastSeen = 0;
  uint32_t outLast = 0;
  portENTER_CRITICAL(&s_mux);
  ok = s_ok;
  outOk = s_outOk;
  temp = s_tempC;
  outTemp = s_outTempC;
  lastSeen = s_lastSeenMs;
  outLast = s_outLastSeenMs;
  portEXIT_CRITICAL(&s_mux);

  if (ok && (millis() - lastSeen) > kStaleMs) {
    portENTER_CRITICAL(&s_mux);
    s_ok = false;
    s_tempC = UI_TEPLOTA_NEPLATNA;
    portEXIT_CRITICAL(&s_mux);
    ok = false;
    temp = UI_TEPLOTA_NEPLATNA;
    Serial.println("[ROOM] timeout — zadna data z H2 (pokoj)");
  }
  if (outOk && (millis() - outLast) > kStaleMs) {
    portENTER_CRITICAL(&s_mux);
    s_outOk = false;
    s_outTempC = UI_TEPLOTA_NEPLATNA;
    portEXIT_CRITICAL(&s_mux);
    outOk = false;
    outTemp = UI_TEPLOTA_NEPLATNA;
    Serial.println("[ROOM] timeout — zadna data z H2 (venku)");
  }

  uiEez.sig_ble = ok || outOk;
  uiEez.teplota_vnitrni = temp;
  uiEez.teplota_venkovni = outTemp;
}

void climateRoomRequestNow(void) {
  sendCmd(H2_CMD_POLL);
}

void climateRoomStartScan(void) {
  if (s_scanBusy) {
    Serial.println("[ROOM] scan already busy");
    return;
  }
  portENTER_CRITICAL(&s_mux);
  s_foundCount = 0;
  for (int i = 0; i < H2_FOUND_MAX; ++i) {
    s_found[i] = ClimateRoomFound{};
  }
  portEXIT_CRITICAL(&s_mux);
  s_scanBusy = true;
  s_scanStartedMs = millis();
  strncpy(uiEez.set_sys_hint, "Skenuji SwitchBot...",
          sizeof(uiEez.set_sys_hint) - 1);
  uiEez.set_sys_hint[sizeof(uiEez.set_sys_hint) - 1] = '\0';
  sendCmd(H2_CMD_SCAN);
}

bool climateRoomSelectMeter(uint8_t index1) {
  if (index1 < 1 || index1 > H2_FOUND_MAX) {
    return false;
  }
  ClimateRoomFound found{};
  if (!climateRoomGetFound(index1, &found) || !found.valid) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "SET ROOM=%u", (unsigned)index1);
    sendCmd(cmd);
    return true;
  }
  return climateRoomSetRoomMac(found.mac);
}

bool climateRoomSetRoomMac(const char* mac) {
  if (!looksLikeMac(mac)) {
    return false;
  }
  storageSaveBleRoomMac(mac);
  strncpy(s_cfgMac, mac, sizeof(s_cfgMac) - 1);
  s_cfgMac[sizeof(s_cfgMac) - 1] = '\0';
  s_cfgMacLoaded = true;
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "SET ROOM=%s", mac);
  sendCmd(cmd);
  return true;
}

bool climateRoomSetOutdoorMac(const char* mac) {
  if (!looksLikeMac(mac)) {
    return false;
  }
  storageSaveBleOutdoorMac(mac);
  strncpy(s_cfgOutMac, mac, sizeof(s_cfgOutMac) - 1);
  s_cfgOutMac[sizeof(s_cfgOutMac) - 1] = '\0';
  s_cfgMacLoaded = true;
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "SET OUT=%s", mac);
  sendCmd(cmd);
  return true;
}

bool climateRoomPushConfig(void) {
  pushStoredMacToH2();
  return true;
}

bool climateRoomIsOk(void) {
  portENTER_CRITICAL(&s_mux);
  const bool ok = s_ok;
  portEXIT_CRITICAL(&s_mux);
  return ok;
}

bool climateRoomIsBusy(void) {
  return s_scanBusy;
}

void climateRoomReleaseForTls(void) {}

bool climateRoomBootPollPending(void) {
  return false;
}

int climateRoomFoundCount(void) {
  portENTER_CRITICAL(&s_mux);
  const int n = s_foundCount;
  portEXIT_CRITICAL(&s_mux);
  return n;
}

bool climateRoomGetFound(uint8_t index1, ClimateRoomFound* out) {
  if (!out || index1 < 1 || index1 > H2_FOUND_MAX) {
    return false;
  }
  portENTER_CRITICAL(&s_mux);
  *out = s_found[index1 - 1];
  portEXIT_CRITICAL(&s_mux);
  return out->valid;
}

void climateRoomGetConfiguredMac(char* buf, size_t len) {
  if (!buf || len == 0) {
    return;
  }
  if (!s_cfgMacLoaded) {
    refreshCfgMacCache();
  }
  if (!isZeroMac(s_cfgMac)) {
    strncpy(buf, s_cfgMac, len - 1);
    buf[len - 1] = '\0';
  } else {
    strncpy(buf, "---", len);
    buf[len - 1] = '\0';
  }
}

void climateRoomGetConfiguredOutdoorMac(char* buf, size_t len) {
  if (!buf || len == 0) {
    return;
  }
  if (!s_cfgMacLoaded) {
    refreshCfgMacCache();
  }
  if (!isZeroMac(s_cfgOutMac)) {
    strncpy(buf, s_cfgOutMac, len - 1);
    buf[len - 1] = '\0';
  } else {
    strncpy(buf, "---", len);
    buf[len - 1] = '\0';
  }
}

float climateRoomTempC(void) {
  portENTER_CRITICAL(&s_mux);
  const float t = s_tempC;
  portEXIT_CRITICAL(&s_mux);
  return t;
}

float climateRoomHumidity(void) {
  portENTER_CRITICAL(&s_mux);
  const float h = s_hum;
  portEXIT_CRITICAL(&s_mux);
  return h;
}

int climateRoomBatteryPct(void) {
  portENTER_CRITICAL(&s_mux);
  const int b = s_batt;
  portEXIT_CRITICAL(&s_mux);
  return b;
}

int climateRoomRssi(void) {
  portENTER_CRITICAL(&s_mux);
  const int r = s_rssi;
  portEXIT_CRITICAL(&s_mux);
  return r;
}

bool climateRoomOutdoorIsOk(void) {
  portENTER_CRITICAL(&s_mux);
  const bool ok = s_outOk;
  portEXIT_CRITICAL(&s_mux);
  return ok;
}

float climateRoomOutdoorTempC(void) {
  portENTER_CRITICAL(&s_mux);
  const float t = s_outTempC;
  portEXIT_CRITICAL(&s_mux);
  return t;
}

float climateRoomOutdoorHumidity(void) {
  portENTER_CRITICAL(&s_mux);
  const float h = s_outHum;
  portEXIT_CRITICAL(&s_mux);
  return h;
}

int climateRoomOutdoorBatteryPct(void) {
  portENTER_CRITICAL(&s_mux);
  const int b = s_outBatt;
  portEXIT_CRITICAL(&s_mux);
  return b;
}

int climateRoomOutdoorRssi(void) {
  portENTER_CRITICAL(&s_mux);
  const int r = s_outRssi;
  portEXIT_CRITICAL(&s_mux);
  return r;
}

void climateRoomGetLastRoomResponse(char* buf, size_t len) {
  formatSensorLine(buf, len, false);
}

void climateRoomGetLastOutdoorResponse(char* buf, size_t len) {
  formatSensorLine(buf, len, true);
}

void climateRoomStatusText(char* buf, size_t buflen) {
  if (!buf || buflen == 0) {
    return;
  }
  if (s_scanBusy) {
    snprintf(buf, buflen, "Skenuji SwitchBot...");
    return;
  }
  const bool roomOk = climateRoomIsOk();
  const bool outOk = climateRoomOutdoorIsOk();
  if (roomOk && outOk) {
    const int outBat = climateRoomOutdoorBatteryPct();
    if (outBat >= 0) {
      snprintf(buf, buflen, "Pokoj %.1f °C · Venku %.1f °C (bat %d%%)",
               climateRoomTempC(), climateRoomOutdoorTempC(), outBat);
    } else {
      snprintf(buf, buflen, "Pokoj %.1f °C · Venku %.1f °C", climateRoomTempC(),
               climateRoomOutdoorTempC());
    }
    return;
  }
  if (roomOk) {
    snprintf(buf, buflen, "H2 %.1f °C · bat %d%% · rssi %d", climateRoomTempC(),
             climateRoomBatteryPct(), climateRoomRssi());
    return;
  }
  if (outOk) {
    const int outBat = climateRoomOutdoorBatteryPct();
    const int outRssi = climateRoomOutdoorRssi();
    if (outBat >= 0) {
      snprintf(buf, buflen, "Venku %.1f °C · bat %d%% · rssi %d",
               climateRoomOutdoorTempC(), outBat, outRssi);
    } else {
      snprintf(buf, buflen, "Venku %.1f °C", climateRoomOutdoorTempC());
    }
    return;
  }
  char mac[H2_MAC_STR_LEN];
  climateRoomGetConfiguredMac(mac, sizeof(mac));
  const int n = climateRoomFoundCount();
  if (n > 0) {
    snprintf(buf, buflen, "Nalezeno %d - vyber Tepl.1-%d", n, n);
  } else {
    snprintf(buf, buflen, "H2 MAC %s - klepněte Skenuj", mac);
  }
}

bool climateRoomBridgeOtaStartWith(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) {
    s_bridgeOtaState = CLIMATE_BRIDGE_OTA_FAIL;
    s_bridgeOtaFailAtMs = millis() + 12000UL;
    Serial.println("[ROOM] bridge OTA — chybi SSID");
    return false;
  }
  char cmd[160];
  snprintf(cmd, sizeof(cmd), "WIFI\t%s\t%s", ssid, pass ? pass : "");
  sendCmd(cmd);
  s_bridgeOtaState = CLIMATE_BRIDGE_OTA_CONNECTING;
  s_bridgeOtaIp[0] = '\0';
  s_bridgeOtaHost[0] = '\0';
  Serial.printf("[ROOM] bridge OTA start ssid=%s\n", ssid);
  return true;
}

void climateRoomBridgeOtaStart(void) {
  char ssid[33];
  char pass[65];
  if (!netWifiCopyCredentials(ssid, sizeof(ssid), pass, sizeof(pass)) &&
      (!storageLoadWifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass)) ||
       ssid[0] == '\0')) {
    s_bridgeOtaState = CLIMATE_BRIDGE_OTA_FAIL;
    s_bridgeOtaFailAtMs = millis() + 12000UL;
    Serial.println("[ROOM] bridge OTA — chybi WiFi creds Tab5");
    return;
  }
  climateRoomBridgeOtaStartWith(ssid, pass);
}

void climateRoomBridgeOtaStop(void) {
  sendCmd(H2_CMD_WIFI_OFF);
  s_bridgeOtaState = CLIMATE_BRIDGE_OTA_IDLE;
  s_bridgeOtaIp[0] = '\0';
  s_bridgeOtaHost[0] = '\0';
}

ClimateBridgeOtaState climateRoomBridgeOtaState(void) {
  if (s_bridgeOtaState == CLIMATE_BRIDGE_OTA_FAIL && s_bridgeOtaFailAtMs != 0 &&
      millis() >= s_bridgeOtaFailAtMs) {
    s_bridgeOtaState = CLIMATE_BRIDGE_OTA_IDLE;
    s_bridgeOtaFailAtMs = 0;
  }
  return s_bridgeOtaState;
}

const char* climateRoomBridgeOtaIp(void) { return s_bridgeOtaIp; }

const char* climateRoomBridgeOtaHost(void) { return s_bridgeOtaHost; }

void climateRoomRequestBridgeInfo(void) {
  if (!s_inited) {
    return;
  }
  sendCmd(H2_CMD_GET_INFO);
}

bool climateRoomBridgeInfoOk(void) {
  return s_bridgeInfoAtMs != 0 && s_bridgeMac[0] != '\0';
}

const char* climateRoomBridgeMac(void) { return s_bridgeMac; }

uint8_t climateRoomBridgeChannel(void) { return s_bridgeCh; }

bool climateRoomBridgeEspNowOk(void) { return s_bridgeEspNow; }

uint32_t climateRoomBridgeInfoAgeMs(void) {
  if (s_bridgeInfoAtMs == 0) {
    return UINT32_MAX;
  }
  return millis() - s_bridgeInfoAtMs;
}

bool climateRoomLastPwrOk(void) { return s_lastPwrAtMs != 0; }

uint16_t climateRoomLastPwrW(void) { return s_lastPwrW; }

float climateRoomLastPwrKwh(void) { return s_lastPwrKwh; }

uint32_t climateRoomLastPwrAgeMs(void) {
  if (s_lastPwrAtMs == 0) {
    return UINT32_MAX;
  }
  return millis() - s_lastPwrAtMs;
}
