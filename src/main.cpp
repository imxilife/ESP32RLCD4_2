#include <Arduino.h>
#include <Wire.h>
#include <display_bsp.h>
#include <Gui.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <GuiTests.h>
#include <WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <app_message.h>
#include <ui_clock.h>
#include <app_tasks.h>

#define ENABLE_GUI_TESTS 0

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

RTC85063  rtc;
Humiture  humiture;
WiFiConfig wifiConfig;

QueueHandle_t g_msgQueue = nullptr;

static constexpr int kTouchIntPin  = -1; // TODO: 设置为真实触摸中断引脚
static constexpr int kButtonIntPin = -1; // TODO: 设置为真实按键中断引脚

// ── 外部中断 ISR ──────────────────────────────────────────────

void IRAM_ATTR onTouchISR() {
    if (g_msgQueue == nullptr) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type    = MSG_TOUCH_EVENT;
    msg.touch.x = 0; // TODO: 根据触摸芯片读取真实坐标
    msg.touch.y = 0;
    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

void IRAM_ATTR onButtonISR() {
    if (g_msgQueue == nullptr) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type         = MSG_BUTTON_EVENT;
    msg.button.btnId = 0; // TODO: 区分不同按键
    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

// WiFiConfig 文本提示回调：将其转换为 WiFi UI 消息投入队列
void wifiUiMessageHandler(const char* line1, const char* line2) {
    if (g_msgQueue == nullptr) return;
    AppMessage msg;
    msg.type = MSG_WIFI_UI;
    memset(msg.wifiUi.line1, 0, WIFI_UI_LINE_MAX);
    memset(msg.wifiUi.line2, 0, WIFI_UI_LINE_MAX);
    if (line1 != nullptr) strncpy(msg.wifiUi.line1, line1, WIFI_UI_LINE_MAX - 1);
    if (line2 != nullptr) strncpy(msg.wifiUi.line2, line2, WIFI_UI_LINE_MAX - 1);
    xQueueSend(g_msgQueue, &msg, 0);
}

// NTP 同步成功回调：将时间数据投入队列，由 loop() 统一写入 RTC
void ntpSyncHandler(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday) {
    if (g_msgQueue == nullptr) return;
    AppMessage msg;
    msg.type             = MSG_NTP_SYNC;
    msg.ntpTime.year     = year;
    msg.ntpTime.month    = month;
    msg.ntpTime.day      = day;
    msg.ntpTime.hour     = hour;
    msg.ntpTime.minute   = minute;
    msg.ntpTime.second   = second;
    msg.ntpTime.weekday  = weekday;
    xQueueSend(g_msgQueue, &msg, 0);
}

// ── 消息处理 ──────────────────────────────────────────────────

static void handleHumiture(float temperature, float humidity) {
    Serial.printf("温度: %.1f °C  湿度: %.1f %%\n", temperature, humidity);

    char tempStr[16], humidStr[16];
    snprintf(tempStr,  sizeof(tempStr),  "%.1fC",  temperature);
    snprintf(humidStr, sizeof(humidStr), "%.1f%%", humidity);

    int tempY  = 300 - 32 - 20;
    int humidX = 400 - (int)strlen(humidStr) * 24 - 20;
    gui.fillRect(0, tempY - 5, 400, 32 + 10, ColorWhite);
    // TODO: gui.drawSmallDigits(20, tempY, tempStr, ColorBlack);
    // TODO: gui.drawSmallDigits(humidX, tempY, humidStr, ColorBlack);
    (void)humidX;
}

static void handleWifiUi(const AppMessage &msg) {
    gui.fillRect(0, 100, 400, 100, ColorWhite);
    // TODO: gui.drawUTF8String(10, 110, msg.wifiUi.line1, ColorBlack);
    // TODO: gui.drawSmallDigits(10, 140, msg.wifiUi.line2, ColorBlack);
    (void)msg;
}

// ── setup / loop ──────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // I2C 总线初始化（RTC PCF85063 和温湿度传感器 SHTC3 共用，SDA=13, SCL=14）
    // 在任务启动前统一初始化，避免各任务并发初始化 Wire 导致竞态
    Wire.begin(13, 14);

    // LCD 初始化
    RlcdPort.RLCD_Init();
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);
    gui.clear();
    gui.display(); // 立即刷屏，确保屏幕就绪

#if ENABLE_GUI_TESTS
    GuiTests::runAllTests(gui);
    delay(5000);
    gui.clear();
    gui.display();
#endif

    // 消息队列：主 loop 作为 UI 事件循环
    g_msgQueue = xQueueCreate(16, sizeof(AppMessage));
    if (g_msgQueue == nullptr) {
        Serial.println("创建消息队列失败！系统无法继续运行");
        while (1) delay(100);
    }

    // 注册 WiFi 文本提示回调（配网进度 → 消息队列 → UI）
    wifiConfig.setMessageCallback(wifiUiMessageHandler);

    // 注册 NTP 同步成功回调（NTP 时间 → 消息队列 → loop() 写入 RTC）
    wifiConfig.setNTPCallback(ntpSyncHandler);

    // 注册外部中断（如有实际引脚）
    if (kTouchIntPin >= 0) {
        pinMode(kTouchIntPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kTouchIntPin), onTouchISR, FALLING);
    }
    if (kButtonIntPin >= 0) {
        pinMode(kButtonIntPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kButtonIntPin), onButtonISR, FALLING);
    }

    // 创建后台任务（各任务负责自身外设初始化，失败时通过消息队列上报）
    xTaskCreate(rtcTask,      "rtcTask",  2048, nullptr, 2, nullptr);
    xTaskCreate(humitureTask, "humTask",  2048, nullptr, 1, nullptr);
    xTaskCreate(wifiTask,     "wifiTask", 8192, nullptr, 2, nullptr);
}

// 主线程：作为 UI Looper，只处理消息并更新界面
void loop() {
    if (g_msgQueue == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    AppMessage msg;
    // 等待任意模块投递的 UI 消息（带超时，便于未来空闲逻辑扩展）
    if (xQueueReceive(g_msgQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) return;

    switch (msg.type) {
    case MSG_RTC_UPDATE:
        handleRtcUpdate(msg.rtcTime);
        break;
    case MSG_HUMITURE_UPDATE:
        handleHumiture(msg.humiture.temp, msg.humiture.hum);
        break;
    case MSG_WIFI_STATUS:
        if (!msg.wifi.connected) Serial.println("WiFi 连接断开");
        break;
    case MSG_WIFI_UI:
        handleWifiUi(msg);
        break;
    case MSG_NTP_SYNC:
        // NTP 同步成功，将时间写入 RTC
        rtc.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                    msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                    msg.ntpTime.weekday);
        Serial.println("时间已写入 RTC");
        break;
    case MSG_TOUCH_EVENT:
        Serial.printf("触摸事件: x=%d, y=%d\n", msg.touch.x, msg.touch.y);
        break;
    case MSG_BUTTON_EVENT:
        Serial.printf("按键事件: id=%d\n", msg.button.btnId);
        break;
    default:
        break;
    }

    // 统一在主线程刷屏，避免多任务并发操作 GUI
    gui.display();
}
