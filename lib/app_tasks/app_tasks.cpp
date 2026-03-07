#include "app_tasks.h"
#include <app_message.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <Arduino.h>
#include <algorithm>

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
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ── 电池 ADC 采样任务 ─────────────────────────────────────────
//
// GPIO4 = ADC1_CH3，通过分压电阻接电池正极
// kBattDividerRatio：分压比，即 Vbat / Vadc
//   例：100kΩ + 100kΩ 分压 → ratio = 2.0
//   请根据实际原理图调整此值
//
// 采样算法：截断均值（Trimmed Mean）
//   1. 连续采集 kSampleCount 次原始 ADC 毫伏值
//   2. 排序后去掉最高 / 最低各 kTrimCount 个（抗毛刺）
//   3. 对中间样本求均值，换算为电池电压（V）
static constexpr int   kBattPin         = 4;    // GPIO4
static constexpr float kBattDividerRatio = 3.0f; // 分压比：R21=200kΩ, R23=100kΩ → Vbat/Vadc = (200+100)/100 = 3.0
static constexpr int   kSampleCount     = 32;   // 总采样次数
static constexpr int   kTrimCount       = 4;    // 首尾各去掉的样本数

static float sampleBatteryVoltage() {
    uint32_t samples[kSampleCount];
    for (int i = 0; i < kSampleCount; i++) {
        samples[i] = (uint32_t)analogReadMilliVolts(kBattPin);
        vTaskDelay(pdMS_TO_TICKS(2)); // 每次采样间隔 2 ms，降低噪声相关性
    }
    std::sort(samples, samples + kSampleCount);

    // 截断均值：去掉首尾各 kTrimCount 个
    uint64_t sum = 0;
    int validCount = kSampleCount - 2 * kTrimCount;
    for (int i = kTrimCount; i < kSampleCount - kTrimCount; i++) {
        sum += samples[i];
    }
    float adcMv  = (float)sum / validCount;
    float voltV  = adcMv * kBattDividerRatio / 1000.0f;
    return voltV;
}

void batteryTask(void* pvParameters) {
    (void)pvParameters;

    // ADC 衰减已在 setup() 中配置，此处直接采样
    // 启动后稍等，让系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true) {
        float voltage = sampleBatteryVoltage();

        Serial.printf("[Battery] %.2f V\n", voltage);

        if (g_msgQueue != nullptr) {
            AppMessage msg;
            msg.type            = MSG_BATTERY_UPDATE;
            msg.battery.voltage = voltage;
            xQueueSend(g_msgQueue, &msg, 0);
        }

        // 每 30 秒采样一次（电池电压变化缓慢，无需高频）
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
