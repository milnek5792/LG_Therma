// Waveshare ESP32-S3-Relay-1CH — PZEM-004T TTL (GPIO1 RX / GPIO2 TX) → ESP-NOW + OTA
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "espnow_energy_config.h"
#include "pzem_ota_config.h"
#include "wifi_config.h"

#ifndef PZEM_RX_PIN
#define PZEM_RX_PIN 1
#endif
#ifndef PZEM_TX_PIN
#define PZEM_TX_PIN 2
#endif
#ifndef PZEM_UART_BAUD
#define PZEM_UART_BAUD 9600
#endif
#ifndef PZEM_ADDR
#define PZEM_ADDR 0xF8
#endif
#ifndef PZEM_ENERGY_RESET_WH
#define PZEM_ENERGY_RESET_WH 9000000u  // 9000 kWh
#endif

namespace {

HardwareSerial PzemSerial(1);

constexpr uint32_t kSampleMs = 1000;
constexpr uint32_t kSendMs = 60000;
constexpr size_t kAvgSlots = 60;

uint16_t s_samples[kAvgSlots];
size_t s_sampleCount = 0;
uint32_t s_lastSampleMs = 0;
uint32_t s_lastSendMs = 0;
uint8_t s_seq = 0;
bool s_resetPendingFlag = false;
bool s_espNowOk = false;
bool s_otaReady = false;

uint8_t s_peerMac[6] = {
    ESPNOW_ENERGY_PEER_MAC0, ESPNOW_ENERGY_PEER_MAC1, ESPNOW_ENERGY_PEER_MAC2,
    ESPNOW_ENERGY_PEER_MAC3, ESPNOW_ENERGY_PEER_MAC4, ESPNOW_ENERGY_PEER_MAC5,
};

uint16_t modbusCrc(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      if (crc & 1u) {
        crc = (uint16_t)((crc >> 1) ^ 0xA001u);
      } else {
        crc = (uint16_t)(crc >> 1);
      }
    }
  }
  return crc;
}

uint32_t be32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

bool pzemTransact(const uint8_t* req, size_t reqLen, uint8_t* resp, size_t respLen,
                  uint32_t timeoutMs) {
  while (PzemSerial.available() > 0) {
    (void)PzemSerial.read();
  }
  PzemSerial.write(req, reqLen);
  PzemSerial.flush();

  size_t got = 0;
  const uint32_t start = millis();
  while (got < respLen && (millis() - start) < timeoutMs) {
    if (PzemSerial.available() > 0) {
      resp[got++] = (uint8_t)PzemSerial.read();
    } else {
      delay(1);
    }
  }
  if (got < respLen) {
    return false;
  }
  const uint16_t crc = modbusCrc(resp, respLen - 2);
  const uint16_t gotCrc =
      (uint16_t)resp[respLen - 2] | ((uint16_t)resp[respLen - 1] << 8);
  return crc == gotCrc;
}

bool pzemReadPowerEnergy(float* outPowerW, uint32_t* outEnergyWh) {
  uint8_t req[8] = {PZEM_ADDR, 0x04, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00};
  const uint16_t crc = modbusCrc(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  uint8_t resp[13];
  if (!pzemTransact(req, sizeof(req), resp, sizeof(resp), 200)) {
    return false;
  }
  if (resp[0] != PZEM_ADDR || resp[1] != 0x04 || resp[2] != 0x08) {
    return false;
  }

  const uint32_t powerRaw = be32(&resp[3]);
  const uint32_t energyWh = be32(&resp[7]);
  if (outPowerW) {
    *outPowerW = (float)powerRaw / 10.0f;
  }
  if (outEnergyWh) {
    *outEnergyWh = energyWh;
  }
  return true;
}

bool pzemResetEnergy(void) {
  uint8_t req[4] = {PZEM_ADDR, 0x42, 0x00, 0x00};
  const uint16_t crc = modbusCrc(req, 2);
  req[2] = (uint8_t)(crc & 0xFF);
  req[3] = (uint8_t)(crc >> 8);

  uint8_t resp[4];
  if (!pzemTransact(req, sizeof(req), resp, sizeof(resp), 300)) {
    return false;
  }
  return resp[0] == PZEM_ADDR && resp[1] == 0x42;
}

bool isBroadcastPeer(void) {
  for (int i = 0; i < 6; ++i) {
    if (s_peerMac[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

uint8_t currentWifiChannel(void) {
  uint8_t primary = ESPNOW_ENERGY_CHANNEL;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK || primary == 0) {
    return ESPNOW_ENERGY_CHANNEL;
  }
  return primary;
}

bool initEspNow(void) {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] init fail");
    s_espNowOk = false;
    return false;
  }

  if (esp_now_is_peer_exist(s_peerMac)) {
    esp_now_del_peer(s_peerMac);
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, s_peerMac, 6);
  // 0 = aktuální Wi‑Fi kanál (po STA connect), jinak fixní z configu
  peer.channel = (WiFi.status() == WL_CONNECTED) ? 0 : ESPNOW_ENERGY_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ESPNOW] add peer fail");
    s_espNowOk = false;
    return false;
  }

  s_espNowOk = true;
  Serial.printf("[ESPNOW] ok ch=%u peer=%02X:%02X:%02X:%02X:%02X:%02X%s\n",
                (unsigned)currentWifiChannel(), s_peerMac[0], s_peerMac[1],
                s_peerMac[2], s_peerMac[3], s_peerMac[4], s_peerMac[5],
                isBroadcastPeer() ? " (broadcast)" : "");
  return true;
}

void beginOta(void) {
  ArduinoOTA.setHostname(PZEM_OTA_HOSTNAME);
  if (PZEM_OTA_PASSWORD[0] != '\0') {
    ArduinoOTA.setPassword(PZEM_OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() { Serial.println("[OTA] start"); });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] done — restart");
    delay(200);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] error %u\n", (unsigned)err);
  });
  ArduinoOTA.begin();
  s_otaReady = true;
  Serial.printf("[OTA] ready host=%s ip=%s port=3232\n", PZEM_OTA_HOSTNAME,
                WiFi.localIP().toString().c_str());
}

bool connectWifiForOta(void) {
  if (WIFI_SSID[0] == '\0' || strcmp(WIFI_SSID, "Vase_Sit") == 0) {
    Serial.println("[WIFI] SSID not set in wifi_config.h — OTA skipped");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(PZEM_OTA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WIFI] connecting ssid=%s\n", WIFI_SSID);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < PZEM_WIFI_CONNECT_MS) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] connect fail — ESP-NOW only");
    WiFi.disconnect(false, false);
    esp_wifi_set_channel(ESPNOW_ENERGY_CHANNEL, WIFI_SECOND_CHAN_NONE);
    return false;
  }

  Serial.printf("[WIFI] ok ip=%s ch=%u\n", WiFi.localIP().toString().c_str(),
                (unsigned)currentWifiChannel());
  beginOta();
  return true;
}

void pushSample(float powerW) {
  uint16_t w = 0;
  if (powerW > 0.0f) {
    if (powerW > 65535.0f) {
      w = 65535;
    } else {
      w = (uint16_t)(powerW + 0.5f);
    }
  }
  if (s_sampleCount < kAvgSlots) {
    s_samples[s_sampleCount++] = w;
  } else {
    memmove(&s_samples[0], &s_samples[1], (kAvgSlots - 1) * sizeof(uint16_t));
    s_samples[kAvgSlots - 1] = w;
  }
}

uint16_t averagePowerW(void) {
  if (s_sampleCount == 0) {
    return 0;
  }
  uint32_t sum = 0;
  for (size_t i = 0; i < s_sampleCount; ++i) {
    sum += s_samples[i];
  }
  return (uint16_t)(sum / (uint32_t)s_sampleCount);
}

void sendMinute(uint32_t energyWh) {
  if (!s_espNowOk) {
    return;
  }
  EspNowEnergyPacket pkt{};
  pkt.magic = ESPNOW_ENERGY_MAGIC;
  pkt.avg_power_w = averagePowerW();
  pkt.energy_wh = energyWh;
  pkt.flags = s_resetPendingFlag ? ESPNOW_ENERGY_FLAG_RESET : 0;
  pkt.seq = ++s_seq;
  s_resetPendingFlag = false;

  const esp_err_t err =
      esp_now_send(s_peerMac, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
  Serial.printf("[ESPNOW] send W=%u E=%lu Wh flags=0x%02X seq=%u err=%d\n",
                (unsigned)pkt.avg_power_w, (unsigned long)pkt.energy_wh,
                (unsigned)pkt.flags, (unsigned)pkt.seq, (int)err);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[PZEM] S3 Relay TTL meter boot");

  PzemSerial.begin(PZEM_UART_BAUD, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);

  if (!connectWifiForOta()) {
    // Bez AP: drž fixní kanál pro ESP-NOW
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(ESPNOW_ENERGY_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }

  if (!initEspNow()) {
    Serial.println("[PZEM] ESP-NOW unavailable — still sampling locally");
  }

  s_lastSampleMs = millis();
  s_lastSendMs = millis();
}

void loop() {
  if (s_otaReady) {
    ArduinoOTA.handle();
  }

  const uint32_t now = millis();

  if ((now - s_lastSampleMs) >= kSampleMs) {
    s_lastSampleMs = now;
    float powerW = 0.0f;
    uint32_t energyWh = 0;
    if (pzemReadPowerEnergy(&powerW, &energyWh)) {
      pushSample(powerW);
      if (energyWh >= PZEM_ENERGY_RESET_WH) {
        if (pzemResetEnergy()) {
          s_resetPendingFlag = true;
          Serial.printf("[PZEM] energy reset at %lu Wh\n",
                        (unsigned long)energyWh);
          energyWh = 0;
        } else {
          Serial.println("[PZEM] energy reset FAILED");
        }
      }
    } else {
      Serial.println("[PZEM] read fail");
    }
  }

  if ((now - s_lastSendMs) >= kSendMs) {
    s_lastSendMs = now;
    float powerW = 0.0f;
    uint32_t energyWh = 0;
    if (!pzemReadPowerEnergy(&powerW, &energyWh)) {
      energyWh = 0;
    } else {
      pushSample(powerW);
    }
    sendMinute(energyWh);
    s_sampleCount = 0;
  }

  delay(5);
}
