// app_cmd.cpp — fronta AppMsg (depth 16)
#include "app_cmd.h"

#include "ui_eez_nav.h"
#include "ui_eez_plan.h"
#include "ui_net_sync.h"
#include "ui_bus_bindings.h"
#include "climate_plan.h"

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/queue.h>

namespace {

static const char* TAG = "APP_CMD";
constexpr UBaseType_t kQueueDepth = 16;

QueueHandle_t s_q = nullptr;

bool isCtrlHmiAction(UiAkceTlacitko akce) {
  switch (akce) {
    case UI_AKCE_START_STOP:
    case UI_AKCE_START:
    case UI_AKCE_STOP:
    case UI_AKCE_TEPLOTA_PLUS:
    case UI_AKCE_TEPLOTA_MINUS:
    case UI_AKCE_REZIM_PREPNOUT:
      return true;
    default:
      return false;
  }
}

bool isCtrlMsg(const AppMsg& msg) {
  switch (msg.cmd) {
    case APP_CMD_POWER_START:
    case APP_CMD_POWER_STOP:
    case APP_CMD_SETPOINT_ABS:
    case APP_CMD_SETPOINT_DELTA:
    case APP_CMD_SET_MODE:
      return true;
    case APP_CMD_HMI_ACTION:
      return isCtrlHmiAction(static_cast<UiAkceTlacitko>(msg.arg));
    default:
      return false;
  }
}

void requeueHeld(const AppMsg* held, int n) {
  for (int i = 0; i < n; ++i) {
    if (xQueueSend(s_q, &held[i], 0) != pdTRUE) {
      ESP_LOGW(TAG, "requeue drop cmd=%u", (unsigned)held[i].cmd);
    }
  }
}

}  // namespace

void appCmdInit(void) {
  if (s_q) {
    return;
  }
  s_q = xQueueCreate(kQueueDepth, sizeof(AppMsg));
  if (!s_q) {
    ESP_LOGE(TAG, "xQueueCreate FAILED");
  } else {
    ESP_LOGI(TAG, "queue depth=%u", (unsigned)kQueueDepth);
  }
}

bool appCmdEnqueue(const AppMsg* msg) {
  if (!s_q || !msg) {
    return false;
  }
  if (xQueueSend(s_q, msg, 0) == pdTRUE) {
    return true;
  }
  ESP_LOGW(TAG, "queue full cmd=%u — drop", (unsigned)msg->cmd);
  return false;
}

bool appCmdEnqueueHmi(UiAkceTlacitko akce) {
  if (akce == UI_AKCE_ZADNA) {
    return true;
  }
  const AppMsg msg = {APP_CMD_HMI_ACTION, static_cast<int32_t>(akce), UI_SP_SRC_HMI};
  return appCmdEnqueue(&msg);
}

bool appCmdEnqueuePower(bool start, UiSpSource src) {
  const AppMsg msg = {start ? APP_CMD_POWER_START : APP_CMD_POWER_STOP, 0, src};
  return appCmdEnqueue(&msg);
}

bool appCmdEnqueueSetpointAbs(int val, UiSpSource src) {
  const AppMsg msg = {APP_CMD_SETPOINT_ABS, val, src};
  return appCmdEnqueue(&msg);
}

bool appCmdEnqueueAdjust(int delta, UiSpSource src) {
  const AppMsg msg = {APP_CMD_SETPOINT_DELTA, delta, src};
  return appCmdEnqueue(&msg);
}

bool appCmdEnqueueMode(bool roomMode, UiSpSource src) {
  const AppMsg msg = {APP_CMD_SET_MODE, roomMode ? 1 : 0, src};
  return appCmdEnqueue(&msg);
}

UBaseType_t appCmdQueueWaiting(void) {
  if (!s_q) {
    return 0;
  }
  return uxQueueMessagesWaiting(s_q);
}

void appCmdDrainUi(void) {
  if (!s_q) {
    return;
  }

  AppMsg held[16];
  int heldN = 0;
  AppMsg msg{};

  while (xQueueReceive(s_q, &msg, 0) == pdTRUE) {
    if (isCtrlMsg(msg)) {
      if (heldN < 16) {
        held[heldN++] = msg;
      } else {
        ESP_LOGW(TAG, "held overflow — drop ctrl cmd=%u", (unsigned)msg.cmd);
      }
      continue;
    }

    if (msg.cmd != APP_CMD_HMI_ACTION) {
      continue;
    }

    const UiAkceTlacitko akce = static_cast<UiAkceTlacitko>(msg.arg);
    if (akce == UI_AKCE_ZADNA) {
      continue;
    }

    static uint32_t s_lastAkceMs = 0;
    const uint32_t now = millis();
    if (s_lastAkceMs != 0 && (now - s_lastAkceMs) < 220) {
      if (heldN < 16) {
        held[heldN++] = msg;
      }
      continue;
    }
    s_lastAkceMs = now;

    switch (akce) {
      case UI_AKCE_MENU:
        uiNavigateTo(SCREEN_ID_SETTINGS);
        break;
      case UI_AKCE_ZPET:
        uiNavigateTo(SCREEN_ID_MAIN);
        break;
      case UI_AKCE_WIFI_EDIT:
        uiNavigateTo(SCREEN_ID_WIFI_SETUP);
        break;
      case UI_AKCE_WIFI_FORM_BACK:
        uiNavigateTo(SCREEN_ID_SETTINGS);
        break;
      case UI_AKCE_WIFI_FORM_SAVE:
        uiNetHandleAction(akce);
        uiNavigateTo(SCREEN_ID_SETTINGS);
        break;
      case UI_AKCE_WIFI_FORM_CONNECT:
        uiNavigateTo(SCREEN_ID_SETTINGS);
        uiNetHandleAction(akce);
        break;
      case UI_AKCE_WIFI_TOGGLE:
      case UI_AKCE_WIFI_CONNECT:
      case UI_AKCE_MQTT_TOGGLE:
      case UI_AKCE_MQTT_CONNECT:
      case UI_AKCE_SETTINGS_BLE:
        uiNetHandleAction(akce);
        break;
      case UI_AKCE_SETTINGS_PLAN:
        uiNavigateTo(SCREEN_ID_PLAN);
        break;
      case UI_AKCE_SETTINGS_SERVIS:
        uiNavigateTo(SCREEN_ID_REGULATOR);
        break;
      case UI_AKCE_PLAN_BACK:
        uiNavigateTo(SCREEN_ID_SETTINGS);
        break;
      case UI_AKCE_PLAN_TOGGLE:
        g_planConfig.aktivni = !g_planConfig.aktivni;
        uiPlanMarkDirty();
        uiPlanRefreshAll();
        break;
      default:
        if (heldN < 16) {
          held[heldN++] = msg;
        }
        break;
    }
  }

  requeueHeld(held, heldN);
}

void appCmdDrainCtrl(void) {
  if (!s_q) {
    return;
  }

  AppMsg held[16];
  int heldN = 0;
  AppMsg msg{};
  static uint32_t s_lastHmiCtrlMs = 0;

  while (xQueueReceive(s_q, &msg, 0) == pdTRUE) {
    if (!isCtrlMsg(msg)) {
      if (heldN < 16) {
        held[heldN++] = msg;
      } else {
        ESP_LOGW(TAG, "held overflow — drop ui cmd=%u", (unsigned)msg.cmd);
      }
      continue;
    }

    if (msg.cmd == APP_CMD_HMI_ACTION) {
      const uint32_t now = millis();
      if (s_lastHmiCtrlMs != 0 && (now - s_lastHmiCtrlMs) < 220) {
        if (heldN < 16) {
          held[heldN++] = msg;
        }
        continue;
      }
      s_lastHmiCtrlMs = now;
    }

    uiBusProcessAppMsg(&msg);
  }

  requeueHeld(held, heldN);
}
