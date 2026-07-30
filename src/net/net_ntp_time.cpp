#include "net_ntp_time.h"

#include "net_wifi_mgr.h"
#include "time_config.h"
#include "ui_eez_model.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

namespace {

enum class NtpPhase : uint8_t {
  kIdle = 0,
  kWaiting,
  kSynced,
  kFailed,
};

NtpPhase s_phase = NtpPhase::kIdle;
bool s_tzReady = false;
bool s_lastWifiConnected = false;
unsigned long s_waitStartedMs = 0;
unsigned long s_lastUiUpdateMs = 0;
unsigned long s_lastResyncMs = 0;
unsigned long s_wifiOkSinceMs = 0;  // od kdy je Wi-Fi stabilně up

char s_status[32] = "Inicializace";
char s_lastCas[8] = "";
char s_lastDatum[12] = "";

constexpr unsigned long kNtpTimeoutMs = 60000;
constexpr unsigned long kUiUpdateMs = 1000;
constexpr unsigned long kSettleAfterWifiMs = 3500;  // nechat RGB/RF dojít klidu
constexpr unsigned long kResyncIntervalMs =
    (unsigned long)NTP_RESYNC_HOURS * 60UL * 60UL * 1000UL;
constexpr time_t kMinValidTime = 946684800;  // 2000-01-01

void setStatus(const char* text) {
  strncpy(s_status, text ? text : "", sizeof(s_status));
  s_status[sizeof(s_status) - 1] = '\0';
}

void ensureTimezone() {
  if (s_tzReady) {
    return;
  }
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
    if (strcmp(s_lastCas, "--:--") != 0) {
      strncpy(s_lastCas, "--:--", sizeof(s_lastCas));
      strncpy(s_lastDatum, "--.--.----", sizeof(s_lastDatum));
      uiEezNastavCas("--:--", "--.--.----", false);
    } else {
      uiEez.cas_platny = false;
    }
    return;
  }

  struct tm tmLocal;
  localtime_r(&now, &tmLocal);

  char cas[8];
  char datum[12];
  snprintf(cas, sizeof(cas), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
  snprintf(datum, sizeof(datum), "%02d.%02d.%04d",
           tmLocal.tm_mday, tmLocal.tm_mon + 1, tmLocal.tm_year + 1900);

  if (strcmp(cas, s_lastCas) != 0 || strcmp(datum, s_lastDatum) != 0 ||
      uiEez.cas_platny != ntpOk) {
    strncpy(s_lastCas, cas, sizeof(s_lastCas));
    s_lastCas[sizeof(s_lastCas) - 1] = '\0';
    strncpy(s_lastDatum, datum, sizeof(s_lastDatum));
    s_lastDatum[sizeof(s_lastDatum) - 1] = '\0';
    uiEezNastavCas(cas, datum, ntpOk);
  }
}

void startSntp() {
  if (!netWifiIsConnected()) {
    setStatus("Ceka na Wi-Fi");
    return;
  }
  if (netWifiIsBusy()) {
    setStatus("Ceka na Wi-Fi");
    return;
  }
  if (s_wifiOkSinceMs == 0 ||
      (millis() - s_wifiOkSinceMs) < kSettleAfterWifiMs) {
    setStatus("Ceka na stabilitu...");
    return;
  }
  // Už běží / hotovo — nespouštěj znovu (blikání NTP: OK ↔ ...)
  if (s_phase == NtpPhase::kWaiting || s_phase == NtpPhase::kSynced) {
    return;
  }

  ensureTimezone();
  configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  s_phase = NtpPhase::kWaiting;
  s_waitStartedMs = millis();
  setStatus("Synchronizace NTP...");
  Serial.println("[NTP] start SNTP (po settle)");
}

}  // namespace

void netNtpInit() {
  ensureTimezone();
  pushTimeToUi(false);
  s_lastUiUpdateMs = millis();
  setStatus("Ceka na Wi-Fi");
  Serial.println("[NTP] init (bez RTC — cas z NTP)");
}

void netNtpRequestSync() {
  if (!netWifiIsConnected()) {
    setStatus("Ceka na Wi-Fi");
    return;
  }
  if (s_phase == NtpPhase::kSynced || s_phase == NtpPhase::kWaiting) {
    return;
  }
  startSntp();
}

void netNtpTick() {
  const bool wifiConnected = netWifiIsConnected();

  if (wifiConnected && !s_lastWifiConnected) {
    s_wifiOkSinceMs = millis();
    // Po reconnect: sync až po settle (startSntp hlídá)
    if (s_phase == NtpPhase::kFailed) {
      s_phase = NtpPhase::kIdle;
    }
  }
  if (!wifiConnected) {
    s_wifiOkSinceMs = 0;
  }

  if (wifiConnected && s_phase == NtpPhase::kIdle && !netWifiIsBusy()) {
    netNtpRequestSync();
  }
  s_lastWifiConnected = wifiConnected;

  if (wifiConnected && s_phase == NtpPhase::kSynced
      && (millis() - s_lastResyncMs >= kResyncIntervalMs)) {
    s_phase = NtpPhase::kIdle;  // povol resync
    startSntp();
  }

  if (s_phase == NtpPhase::kWaiting) {
    const sntp_sync_status_t syncStatus = sntp_get_sync_status();
    if (syncStatus == SNTP_SYNC_STATUS_COMPLETED) {
      s_phase = NtpPhase::kSynced;
      s_lastResyncMs = millis();
      setStatus("Cas synchronizovan");
      pushTimeToUi(true);
      Serial.println("[NTP] synchronizace OK");
    } else if (millis() - s_waitStartedMs >= kNtpTimeoutMs) {
      s_phase = NtpPhase::kFailed;
      setStatus("NTP timeout");
      Serial.println("[NTP] timeout");
    }
  } else if (s_phase == NtpPhase::kFailed && wifiConnected && !netWifiIsBusy()) {
    if (millis() - s_waitStartedMs >= 30000) {
      s_phase = NtpPhase::kIdle;
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
