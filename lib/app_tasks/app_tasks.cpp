#include "app_tasks.h"
#include <app_message.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

extern RTC85063      rtc;
extern Humiture      humiture;
extern WiFiConfig    wifiConfig;
extern QueueHandle_t g_msgQueue;

void rtcTask(void* pvParameters) {
    (void)pvParameters;

    // RTC 初始化（Wire 已在 setup() 中初始化）
    rtc.begin(13, 14);

    while (true) {
        RTCTime now = rtc.now();
        rtc.alarmListener();

        if (g_msgQueue != nullptr) {
            AppMessage msg;
            msg.type    = MSG_RTC_UPDATE;
            msg.rtcTime = now;
            xQueueSend(g_msgQueue, &msg, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void humitureTask(void* pvParameters) {
    (void)pvParameters;

    // 温湿度传感器初始化，失败时上报错误消息后退出任务
    if (!humiture.begin()) {
        if (g_msgQueue != nullptr) {
            AppMessage msg;
            msg.type = MSG_WIFI_UI;
            strncpy(msg.wifiUi.line1, "温湿度传感器", WIFI_UI_LINE_MAX - 1);
            strncpy(msg.wifiUi.line2, "初始化失败", WIFI_UI_LINE_MAX - 1);
            msg.wifiUi.line1[WIFI_UI_LINE_MAX - 1] = '\0';
            msg.wifiUi.line2[WIFI_UI_LINE_MAX - 1] = '\0';
            xQueueSend(g_msgQueue, &msg, 0);
        }
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        float temperature, humidity;
        if (humiture.read(temperature, humidity)) {
            if (g_msgQueue != nullptr) {
                AppMessage msg;
                msg.type          = MSG_HUMITURE_UPDATE;
                msg.humiture.temp = temperature;
                msg.humiture.hum  = humidity;
                xQueueSend(g_msgQueue, &msg, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void wifiTask(void* pvParameters) {
    (void)pvParameters;

    wifiConfig.begin();

    bool lastConnected = wifiConfig.isConnected();

    if (g_msgQueue != nullptr) {
        AppMessage msg;
        msg.type           = MSG_WIFI_STATUS;
        msg.wifi.connected = lastConnected;
        xQueueSend(g_msgQueue, &msg, 0);
    }

    while (true) {
        bool connected = wifiConfig.isConnected();
        if (connected != lastConnected && g_msgQueue != nullptr) {
            AppMessage msg;
            msg.type           = MSG_WIFI_STATUS;
            msg.wifi.connected = connected;
            xQueueSend(g_msgQueue, &msg, 0);
            lastConnected = connected;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
