#include "app_tasks.h"
#include <core/app_message/app_message.h>
#include <device/rtc/RTC85063.h>
#include <device/humiture/Humiture.h>
#include <features/network/WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <Arduino.h>
#include <algorithm>

extern QueueHandle_t g_msgQueue;
extern SemaphoreHandle_t g_i2cMutex;

void rtcTask(void* pvParameters) {
    RTC85063* rtc = static_cast<RTC85063*>(pvParameters);

    // RTC 初始化（Wire 已在 setup() 中初始化）
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        rtc->begin(13, 14);
        xSemaphoreGive(g_i2cMutex);
    }

    while (true) {
        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            RTCTime now = rtc->now();
            rtc->alarmListener();
            xSemaphoreGive(g_i2cMutex);

            if (g_msgQueue != nullptr) {
                AppMessage msg;
                msg.type    = MSG_RTC_UPDATE;
                msg.rtcTime = now;
                xQueueSend(g_msgQueue, &msg, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ── 自发热补偿参数 ───────────────────────────────────────────
// ESP32-S3 SoC 发热通过 PCB 传导至 SHTC3，温度随运行时间升高。
// 补偿模型：线性插值，开机 0 分钟偏移 0°C，到达稳态后偏移 kMaxCompensation°C。
// kRampMinutes：从冷启动到热稳态的大致时间（分钟）
// kMaxCompensation：热稳态时的温度补偿量（°C），需根据实际对比校准器调整
static constexpr float kMaxCompensation = 4.0f;   // 热稳态补偿（°C），按实测调整
static constexpr float kRampMinutes     = 30.0f;  // 热爬升时间（分钟）

static float calcThermalCompensation() {
    float minutesSinceBoot = (float)millis() / 60000.0f;
    float ratio = minutesSinceBoot / kRampMinutes;
    if (ratio > 1.0f) ratio = 1.0f;
    return kMaxCompensation * ratio;
}

void humitureTask(void* pvParameters) {
    Humiture* humiture = static_cast<Humiture*>(pvParameters);

    // 温湿度传感器初始化，失败时上报错误消息后退出任务
    bool initOk = false;
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        initOk = humiture->begin();
        xSemaphoreGive(g_i2cMutex);
    }
    if (!initOk) {
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
        bool readOk = false;

        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            readOk = humiture->read(temperature, humidity);
            xSemaphoreGive(g_i2cMutex);
        }

        if (readOk) {
            // 自发热补偿：减去因 SoC 发热导致的温度偏移
            float compensation = calcThermalCompensation();
            temperature -= compensation;

            if (g_msgQueue != nullptr) {
                AppMessage msg;
                msg.type          = MSG_HUMITURE_UPDATE;
                msg.humiture.temp = temperature;
                msg.humiture.hum  = humidity;
                xQueueSend(g_msgQueue, &msg, 0);
            }
        }

        // 采样间隔从 5s 增大到 30s，减少传感器自身发热
        vTaskDelay(pdMS_TO_TICKS(30000));
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
    WiFiConfig* wifiConfig = static_cast<WiFiConfig*>(pvParameters);

    wifiConfig->begin();

    bool lastConnected = wifiConfig->isConnected();

    if (g_msgQueue != nullptr) {
        AppMessage msg;
        msg.type           = MSG_WIFI_STATUS;
        msg.wifi.connected = lastConnected;
        xQueueSend(g_msgQueue, &msg, 0);
    }

    while (true) {
        bool connected = wifiConfig->isConnected();
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
