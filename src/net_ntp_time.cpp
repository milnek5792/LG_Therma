#include "net_ntp_time.h"

#include "net_wifi_mgr.h"
#include "net_mqtt_client.h"
#include "time_config.h"
#include "ui_eez_model.h"

#include <M5Unified.h>
#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

namespace {

enum class NtpPhase : uint8_t {
  kIdle = 0,
  kWaiting,
  kSynced,
  kFailed,
};

NtpPhase s_phase = NtpPhase::kIdle;
bool s_tzReady = false;
bool s_sntpStarted = false;
bool s_lastWifiConnected = false;
unsigned long s_waitStartedMs = 0;
unsigned long s_lastUiUpdateMs = 0;
unsigned long s_lastResyncMs = 0;

char s_status[32] = "Inicializace";

constexpr unsigned long kNtpTimeoutMs = 60000;
constexpr unsigned long kUiUpdateMs = 1000;
constexpr unsigned long kResyncIntervalMs =
    (unsigned long)NTP_RESYNC_HOURS * 60UL * 60UL * 1000UL;
constexpr time_t kMinValidTime = 946684800;  // 2000-01-01

void setStatus(const char* text) {
  strncpy(s_status, text ? text : "", sizeof(s_status));
  s_status[sizeof(s_status) - 1] = '\0';
}

void ensureTimezone() {
  if (s_tzReady) { return; }
  setenv("TZ", NTP_TIMEZONE, 1);
  tzset();
  s_tzReady = true;
}

bool timeLooksValid(time_t t) {
  return t >= kMinValidTime;
}

void pushTimeToUi(bool ntpOk) {
  time_t now = time(nullptr);
  if (!timeLooksValid(now)) {
    uiEezNastavCas("--:--", "--.--.----", false);
    return;
  }

  struct tm tmLocal;
  localtime_r(&now, &tmLocal);

  char cas[8];
  char datum[12];
  snprintf(cas, sizeof(cas), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
  snprintf(datum, sizeof(datum), "%02d.%02d.%04d",
            tmLocal.tm_mday, tmLocal.tm_mon + 1, tmLocal.tm_year + 1900);
  uiEezNastavCas(cas, datum, ntpOk);
}

void loadSystemTimeFromRtc() {
  if (!M5.Rtc.isEnabled()) {
    setStatus("RTC nenalezeno");
    Serial.println("[NTP] RTC RX8130 nenalezeno");
    return;
  }

  struct timezone tz = {0, 0};
  M5.Rtc.setSystemTimeFromRtc(&tz);
  if (timeLooksValid(time(nullptr))) {
    setStatus("Cas z RTC");
    pushTimeToUi(false);
    Serial.println("[NTP] systemovy cas nacten z RTC");
  }
}

void syncRtcFromSystemUtc() {
  if (!M5.Rtc.isEnabled()) { return; }

  time_t target = time(nullptr) + 1;
  while (target > time(nullptr)) {
    delay(1);
  }

  struct tm* utc = gmtime(&target);
  if (!utc) { return; }

  M5.Rtc.setDateTime(utc);
  Serial.printf("[NTP] RTC nastaveno UTC %04d-%02d-%02d %02d:%02d:%02d\n",
                utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
                utc->tm_hour, utc->tm_min, utc->tm_sec);
}

void startSntp() {
  if (!netWifiIsConnected()) {
    setStatus("Čeká na Wi-Fi");
    return;
  }

  ensureTimezone();
  configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  s_sntpStarted = true;
  s_phase = NtpPhase::kWaiting;
  s_waitStartedMs = millis();
  setStatus("Synchronizace NTP...");
  Serial.println("[NTP] start SNTP");
}

}  // namespace

void netNtpInit() {
  ensureTimezone();
  loadSystemTimeFromRtc();
  pushTimeToUi(false);
  s_lastUiUpdateMs = millis();
}

void netNtpRequestSync() {
  if (!netWifiIsConnected()) {
    setStatus("Čeká na Wi-Fi");
    return;
  }
  startSntp();
}

void netNtpTick() {
  const bool wifiConnected = netWifiIsConnected();
  if (wifiConnected && !s_lastWifiConnected) {
    netNtpRequestSync();
  }
  s_lastWifiConnected = wifiConnected;

  // Při MQTT nechat SDIO v klidu — NTP resync odložit
  if (wifiConnected && s_phase == NtpPhase::kSynced
      && !netMqttIsConnected()
      && (millis() - s_lastResyncMs >= kResyncIntervalMs)) {
    startSntp();
  }

  if (s_phase == NtpPhase::kWaiting) {
    const sntp_sync_status_t syncStatus = sntp_get_sync_status();
    if (syncStatus == SNTP_SYNC_STATUS_COMPLETED) {
      syncRtcFromSystemUtc();
      s_phase = NtpPhase::kSynced;
      s_lastResyncMs = millis();
      setStatus("Čas synchronizován");
      pushTimeToUi(true);
      Serial.println("[NTP] synchronizace OK");
    } else if (millis() - s_waitStartedMs >= kNtpTimeoutMs) {
      s_phase = NtpPhase::kFailed;
      setStatus("NTP timeout");
      Serial.println("[NTP] timeout");
    }
  } else if (s_phase == NtpPhase::kFailed && wifiConnected && s_sntpStarted) {
    if (millis() - s_waitStartedMs >= 30000) {
      startSntp();
    }
  }

  if (millis() - s_lastUiUpdateMs >= kUiUpdateMs) {
    s_lastUiUpdateMs = millis();
    pushTimeToUi(s_phase == NtpPhase::kSynced);
  }
}

bool netNtpIsSynced() {
  return s_phase == NtpPhase::kSynced;
}

bool netNtpIsWaiting() {
  return s_phase == NtpPhase::kWaiting;
}

const char* netNtpStatus() {
  return s_status;
}
