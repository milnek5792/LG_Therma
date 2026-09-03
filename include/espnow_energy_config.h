// espnow_energy_config.h — ESP-NOW paket spotřeby (S3 PZEM ↔ C3 bridge)
#ifndef ESPNOW_ENERGY_CONFIG_H
#define ESPNOW_ENERGY_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Magic 'PWR1' */
#ifndef ESPNOW_ENERGY_MAGIC
#define ESPNOW_ENERGY_MAGIC 0x31525750u
#endif

/** Wi‑Fi kanál pro ESP-NOW (oba konce musí sedět). */
#ifndef ESPNOW_ENERGY_CHANNEL
#define ESPNOW_ENERGY_CHANNEL 1
#endif

/**
 * MAC peeru C3 bridge (STA). Přepiš build flagem / úpravou po zjištění MAC.
 * Formát: šest bajtů.
 */
#ifndef ESPNOW_ENERGY_PEER_MAC0
#define ESPNOW_ENERGY_PEER_MAC0 0x1C
#define ESPNOW_ENERGY_PEER_MAC1 0xDB
#define ESPNOW_ENERGY_PEER_MAC2 0xD4
#define ESPNOW_ENERGY_PEER_MAC3 0xF0
#define ESPNOW_ENERGY_PEER_MAC4 0xC7
#define ESPNOW_ENERGY_PEER_MAC5 0x58
#endif

/** Flag: Energy na PZEM byla v této minutě resetována. */
#define ESPNOW_ENERGY_FLAG_RESET 0x01u

#pragma pack(push, 1)
typedef struct {
  uint32_t magic;
  uint16_t avg_power_w;
  uint32_t energy_wh;
  uint8_t flags;
  uint8_t seq;
} EspNowEnergyPacket;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
