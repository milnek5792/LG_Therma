// h2_uart_protocol.h — Tab5 (G6/G7) ↔ ESP32-H2 UART @ 115200
#ifndef H2_UART_PROTOCOL_H
#define H2_UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/** Tab5 → H2 */
#define H2_CMD_SCAN     "SCAN"
#define H2_CMD_POLL     "POLL"
#define H2_CMD_GET_CFG  "GET CFG"

/** H2 → Tab5 */
#define H2_PREFIX_FOUND    "FOUND "
#define H2_PREFIX_SCAN_DONE "SCAN DONE"
#define H2_PREFIX_CFG        "CFG "
#define H2_PREFIX_OK         "OK"
#define H2_PREFIX_ERR        "ERR "

#define H2_FOUND_MAX  8
#define H2_MAC_STR_LEN 18  // AA:BB:CC:DD:EE:FF

#ifdef __cplusplus
}
#endif

#endif
