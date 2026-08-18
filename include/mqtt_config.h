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
#define MQTT_TOPIC_TELE_POWER        MQTT_BASE "/tele/power"
#define MQTT_TOPIC_TELE_PUMP         MQTT_BASE "/tele/pump"
#define MQTT_TOPIC_TELE_COMPRESSOR   MQTT_BASE "/tele/compressor"
#define MQTT_TOPIC_TELE_LIN          MQTT_BASE "/tele/lin"
#define MQTT_TOPIC_TELE_WATCH        MQTT_BASE "/tele/watch"
#define MQTT_TOPIC_TELE_ALARM        MQTT_BASE "/tele/alarm"

#define MQTT_TOPIC_CMD_POWER    MQTT_BASE "/cmd/power"
#define MQTT_TOPIC_CMD_SETPOINT MQTT_BASE "/cmd/setpoint"
#define MQTT_TOPIC_CMD_MODE     MQTT_BASE "/cmd/mode"
/** 1 = tele/compressor vždy ON (test MQTT → mobil). Po ověření dej 0. */
#ifndef MQTT_COMPRESSOR_FORCE_ON
#define MQTT_COMPRESSOR_FORCE_ON 0
#endif

/**
 * Test: ON = simuluj běžící kompresor na tele/compressor; OFF = zpět na bus.
 */
#define MQTT_TOPIC_CMD_COMPRESSOR MQTT_BASE "/cmd/compressor"
/**
 * Mobil → zařízení: ON = telemetrie (doporučeně retain=true při otevření panelu).
 * OFF = stop tele (retain u OFF raději vypnout, ať po reconnectu nezůstane „OFF“).
 */
#define MQTT_TOPIC_CMD_WATCH    MQTT_BASE "/cmd/watch"

#ifndef MQTT_TELE_ENABLE
#define MQTT_TELE_ENABLE 1
#endif

/** 1 = tele jen při aktivním watch (cmd/watch + idle timeout). */
#ifndef MQTT_TELE_REQUIRE_WATCH
#define MQTT_TELE_REQUIRE_WATCH 1
#endif

#ifndef MQTT_WATCH_IDLE_MS
#define MQTT_WATCH_IDLE_MS (5 * 60 * 1000)
#endif

/** Interval mezi kroky prvního tele sync po watch ON. */
#ifndef MQTT_TELE_STEP_MS
#define MQTT_TELE_STEP_MS 80
#endif

/** Jak často kontrolovat změny tele (ms) — menší = svižnější panel. */
#ifndef MQTT_TELE_CHANGE_MS
#define MQTT_TELE_CHANGE_MS 200
#endif

/** Neplatná teplota v tele/* (místo "off" — IoT Panel / UI). */
#ifndef MQTT_TELE_NA
#define MQTT_TELE_NA "___"
#endif

#ifndef MQTT_TLS_INSECURE
#define MQTT_TLS_INSECURE 1
#endif

#endif
