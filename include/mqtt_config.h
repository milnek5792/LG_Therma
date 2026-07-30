// mqtt_config.h — EMQX Cloud Serverless (TLS) pro LG Therma 7B
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#ifndef MQTT_HOST
#define MQTT_HOST "n9e16b3c.ala.eu-central-1.emqxsl.com"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif

#ifndef MQTT_USER
#define MQTT_USER "LGThermaTab"
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "u8H7usa84dkzFRc"
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "LGTherma7B"
#endif

#ifndef MQTT_BASE
#define MQTT_BASE "lgtherma"
#endif

#define MQTT_TOPIC_AVAILABILITY MQTT_BASE "/availability"

#define MQTT_TOPIC_TELE_TEMP_ROOM    MQTT_BASE "/tele/temp_room"
#define MQTT_TOPIC_TELE_TEMP_OUTDOOR MQTT_BASE "/tele/temp_outdoor"
#define MQTT_TOPIC_TELE_TEMP_INLET   MQTT_BASE "/tele/temp_inlet"
#define MQTT_TOPIC_TELE_TEMP_OUTLET  MQTT_BASE "/tele/temp_outlet"
#define MQTT_TOPIC_TELE_TEMP_SET     MQTT_BASE "/tele/temp_set"
#define MQTT_TOPIC_TELE_DELTA_T      MQTT_BASE "/tele/delta_t"
#define MQTT_TOPIC_TELE_POWER        MQTT_BASE "/tele/power"
#define MQTT_TOPIC_TELE_PUMP         MQTT_BASE "/tele/pump"
#define MQTT_TOPIC_TELE_COMPRESSOR   MQTT_BASE "/tele/compressor"
#define MQTT_TOPIC_TELE_ELEC_HEAT    MQTT_BASE "/tele/elec_heat"
#define MQTT_TOPIC_TELE_DEFROST      MQTT_BASE "/tele/defrost"
#define MQTT_TOPIC_TELE_QUIET        MQTT_BASE "/tele/quiet"
#define MQTT_TOPIC_TELE_MODE         MQTT_BASE "/tele/mode"
#define MQTT_TOPIC_TELE_ALARM        MQTT_BASE "/tele/alarm"
#define MQTT_TOPIC_TELE_WIFI         MQTT_BASE "/tele/wifi"

#define MQTT_TOPIC_CMD_POWER    MQTT_BASE "/cmd/power"
#define MQTT_TOPIC_CMD_SETPOINT MQTT_BASE "/cmd/setpoint"
#define MQTT_TOPIC_CMD_MODE     MQTT_BASE "/cmd/mode"
#define MQTT_TOPIC_CMD_QUIET    MQTT_BASE "/cmd/quiet"
#define MQTT_TOPIC_CMD_WATCH    MQTT_BASE "/cmd/watch"

#ifndef MQTT_TELE_ENABLE
#define MQTT_TELE_ENABLE 1
#endif

// Po connect: 3 testovací do temp_outlet (retain), pak sync ostatních
#ifndef MQTT_TELE_OUTLET_DEMO
#define MQTT_TELE_OUTLET_DEMO 1
#endif

// Pauza mezi publish při connect sync (ms)
#ifndef MQTT_TELE_STEP_MS
#define MQTT_TELE_STEP_MS 200
#endif

// 1 = přeskočit ověření certifikátu (první ověření spojení)
#ifndef MQTT_TLS_INSECURE
#define MQTT_TLS_INSECURE 1
#endif

#endif
