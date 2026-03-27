#include "NetworkService.h"

#include <core/app_message/app_message.h>
#include <core/app_tasks/app_tasks.h>
#include <features/network/WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

extern QueueHandle_t g_msgQueue;

namespace {
WiFiConfig g_wifiConfig;
bool g_started = false;

void emitWifiUiMessage(const char* line1, const char* line2) {
    if (g_msgQueue == nullptr) return;

    AppMessage msg;
    msg.type = MSG_WIFI_UI;
    memset(msg.wifiUi.line1, 0, WIFI_UI_LINE_MAX);
    memset(msg.wifiUi.line2, 0, WIFI_UI_LINE_MAX);
    if (line1 != nullptr) strncpy(msg.wifiUi.line1, line1, WIFI_UI_LINE_MAX - 1);
    if (line2 != nullptr) strncpy(msg.wifiUi.line2, line2, WIFI_UI_LINE_MAX - 1);
    xQueueSend(g_msgQueue, &msg, 0);
}

void emitNtpSync(uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second,
                 uint8_t weekday) {
    if (g_msgQueue == nullptr) return;

    AppMessage msg;
    msg.type            = MSG_NTP_SYNC;
    msg.ntpTime.year    = year;
    msg.ntpTime.month   = month;
    msg.ntpTime.day     = day;
    msg.ntpTime.hour    = hour;
    msg.ntpTime.minute  = minute;
    msg.ntpTime.second  = second;
    msg.ntpTime.weekday = weekday;
    xQueueSend(g_msgQueue, &msg, 0);
}
}  // namespace

namespace NetworkService {
void begin() {
    if (g_started) return;

    g_started = true;
    g_wifiConfig.setMessageCallback(emitWifiUiMessage);
    g_wifiConfig.setNTPCallback(emitNtpSync);
    xTaskCreate(wifiTask, "wifiTask", 8192, &g_wifiConfig, 2, nullptr);
}

bool isConnected() {
    return g_wifiConfig.isConnected();
}

String ipAddress() {
    return g_wifiConfig.getIPAddress();
}

String ssid() {
    return g_wifiConfig.getSSID();
}
}  // namespace
