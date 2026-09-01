// mqtt_config.h — EMQX Cloud Serverless (TLS)
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#ifndef MQTT_HOST
#define MQTT_HOST "n9e16b3c.ala.eu-central-1.emqxsl.com"
#endif

// Volitelně pevná IPv4 (když DNS na Tab5 zlobí). Zjistíš: nslookup <MQTT_HOST>
// #define MQTT_HOST_IP "x.x.x.x"

#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif

#ifndef MQTT_USER
#define MQTT_USER "LGThermaTab"
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "u8H7usa84dkzFRc"
#endif

// Prefix Client ID — doplní se MAC (stabilní na zařízení, unikátní vs MQTTX).
#ifndef MQTT_CLIENT_ID_PREFIX
#define MQTT_CLIENT_ID_PREFIX "LGThermaTab"
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID MQTT_CLIENT_ID_PREFIX
#endif

/** Čekání na CONNACK / socket I/O (s). 8 s = časté -4 po reconnectu. */
#ifndef MQTT_SOCKET_TIMEOUT_S
#define MQTT_SOCKET_TIMEOUT_S 20
#endif

/** Pauza po hard-stop před novým TLS (ms) — broker uvolní session. */
#ifndef MQTT_RECONNECT_SETTLE_MS
#define MQTT_RECONNECT_SETTLE_MS 2000
#endif

// Korén topiců — Home Assistant / Node-RED / MQTTX
#ifndef MQTT_BASE
#define MQTT_BASE "lgtherma"
#endif

#define MQTT_TOPIC_AVAILABILITY MQTT_BASE "/availability"
// MQTT_TOPIC_STATE (JSON snapshot) — nepoužíváme, jen jednotlivé tele/*

#define MQTT_TOPIC_TELE_TEMP_ROOM    MQTT_BASE "/tele/temp_room"
#define MQTT_TOPIC_TELE_TEMP_OUTDOOR MQTT_BASE "/tele/temp_outdoor"
#define MQTT_TOPIC_TELE_TEMP_INLET   MQTT_BASE "/tele/temp_inlet"
#define MQTT_TOPIC_TELE_TEMP_OUTLET  MQTT_BASE "/tele/temp_outlet"
#define MQTT_TOPIC_TELE_TEMP_SET     MQTT_BASE "/tele/temp_set"
#define MQTT_TOPIC_TELE_REG_MODE     MQTT_BASE "/tele/reg_mode"
#define MQTT_TOPIC_TELE_DELTA_T      MQTT_BASE "/tele/delta_t"
#define MQTT_TOPIC_TELE_POWER        MQTT_BASE "/tele/power"
#define MQTT_TOPIC_TELE_PUMP         MQTT_BASE "/tele/pump"
#define MQTT_TOPIC_TELE_COMPRESSOR   MQTT_BASE "/tele/compressor"
#define MQTT_TOPIC_TELE_LIN          MQTT_BASE "/tele/lin"
#define MQTT_TOPIC_TELE_WATCH        MQTT_BASE "/tele/watch"
#define MQTT_TOPIC_TELE_ELEC_HEAT    MQTT_BASE "/tele/elec_heat"
#define MQTT_TOPIC_TELE_DEFROST      MQTT_BASE "/tele/defrost"
#define MQTT_TOPIC_TELE_QUIET        MQTT_BASE "/tele/quiet"
#define MQTT_TOPIC_TELE_MODE         MQTT_BASE "/tele/mode"
#define MQTT_TOPIC_TELE_ALARM        MQTT_BASE "/tele/alarm"
#define MQTT_TOPIC_TELE_PORUCHA      MQTT_BASE "/tele/porucha"
#define MQTT_TOPIC_TELE_BLE          MQTT_BASE "/tele/ble"

#define MQTT_TOPIC_CMD_POWER    MQTT_BASE "/cmd/power"
#define MQTT_TOPIC_CMD_SETPOINT MQTT_BASE "/cmd/setpoint"
#define MQTT_TOPIC_CMD_MODE     MQTT_BASE "/cmd/mode"
#define MQTT_TOPIC_CMD_QUIET    MQTT_BASE "/cmd/quiet"
#define MQTT_TOPIC_CMD_WATCH    MQTT_BASE "/cmd/watch"
/** Test: ON = simuluj běžící kompresor na tele/compressor. */
#define MQTT_TOPIC_CMD_COMPRESSOR MQTT_BASE "/cmd/compressor"

#ifndef MQTT_COMPRESSOR_FORCE_ON
#define MQTT_COMPRESSOR_FORCE_ON 0
#endif

#ifndef MQTT_TELE_ENABLE
#define MQTT_TELE_ENABLE 1
#endif

#ifndef MQTT_TELE_REQUIRE_WATCH
#define MQTT_TELE_REQUIRE_WATCH 1
#endif

#ifndef MQTT_TELE_STEP_MS
#define MQTT_TELE_STEP_MS 80
#endif

#ifndef MQTT_TELE_CHANGE_MS
#define MQTT_TELE_CHANGE_MS 100
#endif

#ifndef MQTT_TELE_NA
#define MQTT_TELE_NA "___"
#endif

/** Interval telemetrie (ms) — legacy fallback. */
#ifndef MQTT_TELE_INTERVAL_MS
#define MQTT_TELE_INTERVAL_MS 60000
#endif

#ifndef MQTT_TELE_WATCH_INTERVAL_MS
#define MQTT_TELE_WATCH_INTERVAL_MS 15000
#endif

#ifndef MQTT_WATCH_IDLE_MS
#define MQTT_WATCH_IDLE_MS (5 * 60 * 1000)
#endif

/** TLS bez ověření certifikátu (stejný model jako FVE3 / EMQX Cloud).
 *  Spojení zůstává šifrované (8883); CA bundle se nepoužívá. */
#ifndef MQTT_TLS_INSECURE
#define MQTT_TLS_INSECURE 1
#endif

#endif
