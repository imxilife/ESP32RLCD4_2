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
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"
#include "fonts/Font_chinese_Oswald_Light_28_40.h"

#define ENABLE_GUI_TESTS 0

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

RTC85063  rtc;
Humiture  humiture;
WiFiConfig wifiConfig;

QueueHandle_t g_msgQueue = nullptr;

// WiFi/NTP 流程期间置 true，抑制时钟重绘，避免 MSG_RTC_UPDATE 闪入 WiFi 文案
// 归零时机：MSG_NTP_SYNC（成功）或 MSG_WIFI_STATUS（失败/NTP失败）
static bool g_wifiUiVisible = false;

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

// 统计 UTF-8 字符串的 Unicode 码点数（用于计算文本宽度）
static int utf8Count(const char *s) {
    int n = 0;
    while (*s) { if ((*s & 0xC0) != 0x80) n++; s++; }
    return n;
}

static void handleHumiture(float temperature, float humidity) {
#if DISABLED
    Serial.printf("温度: %.1f °C  湿度: %.1f %%\n", temperature, humidity);
#endif

    static const int kAdvX     = 28;              // Oswald Light 28×40 per-char advance
    static const int kFontH    = 40;
    static const int kY        = 300 - kFontH - 16; // 244
    static const int kMargin   = 10;
    static const int kDotR     = 3;               // decimal dot radius
    static const int kDotSep   = 4;               // gap: char-edge ↔ dot-edge
    static const int kDotZoneW = 2 * (kDotSep + kDotR); // 14 px consumed by dot zone
    static const int kDotBaseY = kY + 33;         // decimal dot y ≈ baseline
    static const int kDegR     = 4;               // ° circle radius
    static const int kDegOffX  = 0;               // ° cx offset from left of 'C' cell
    static const int kDegOffY  = 6;               // ° cy offset from top of glyph

    gui.fillRect(0, kY - 4, 400, kFontH + 8, ColorWhite);
    gui.setFont(&kFont_chinese_Oswald_Light_28_40);

    char numStr[16], intBuf[8], fracBuf[8];

    // ── 温度 左对齐："23.0" → "23" + circle + "0C" ──────────────
    snprintf(numStr, sizeof(numStr), "%.1f", temperature);
    {
        char *dot = strchr(numStr, '.');
        int   il  = dot ? (int)(dot - numStr) : (int)strlen(numStr);
        strncpy(intBuf, numStr, il); intBuf[il] = '\0';
        snprintf(fracBuf, sizeof(fracBuf), "%sC", (dot && dot[1]) ? dot + 1 : "0");
    }
    {
        int ix  = kMargin;
        int dcx = ix + (int)strlen(intBuf) * kAdvX + kDotSep + kDotR;
        int fx  = dcx + kDotR + kDotSep;
        gui.drawText(ix,  kY, intBuf,  ColorBlack, ColorWhite);
        gui.fillCircle(dcx, kDotBaseY, kDotR, ColorBlack);
        gui.drawText(fx,  kY, fracBuf, ColorBlack, ColorWhite);
        // Overlay ° circle at top-left of the last 'C' glyph to form °C
        int cCellX = fx + ((int)strlen(fracBuf) - 1) * kAdvX;
        gui.drawCircle(cCellX + kDegOffX, kY + kDegOffY, kDegR, ColorBlack);
    }

    // ── 湿度 右对齐："56.8" → "56" + circle + "8%" ───────────────
    snprintf(numStr, sizeof(numStr), "%.1f", humidity);
    {
        char *dot = strchr(numStr, '.');
        int   il  = dot ? (int)(dot - numStr) : (int)strlen(numStr);
        strncpy(intBuf, numStr, il); intBuf[il] = '\0';
        snprintf(fracBuf, sizeof(fracBuf), "%s%%", (dot && dot[1]) ? dot + 1 : "0");
    }
    {
        int intW = (int)strlen(intBuf) * kAdvX;
        int frW  = (int)strlen(fracBuf) * kAdvX;
        int ix   = 400 - kMargin - intW - kDotZoneW - frW;
        int dcx  = ix + intW + kDotSep + kDotR;
        int fx   = dcx + kDotR + kDotSep;
        gui.drawText(ix,  kY, intBuf,  ColorBlack, ColorWhite);
        gui.fillCircle(dcx, kDotBaseY, kDotR, ColorBlack);
        gui.drawText(fx,  kY, fracBuf, ColorBlack, ColorWhite);
    }
}

static void handleWifiUi(const AppMessage &msg) {
    // 全屏清除，WiFi 文案居中显示；标记 WiFi UI 激活，抑制时钟重绘
    g_wifiUiVisible = true;
    gui.clear();
    gui.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    static const int kFontW   = 20;
    static const int kFontH   = 20;
    static const int kLineGap = 8;
    static const int kScreenW = 400;
    static const int kScreenH = 300;

    bool hasLine2 = msg.wifiUi.line2[0] != '\0';
    int  totalH   = hasLine2 ? (kFontH + kLineGap + kFontH) : kFontH;
    int  startY   = (kScreenH - totalH) / 2;

    if (msg.wifiUi.line1[0] != '\0') {
        int x = (kScreenW - utf8Count(msg.wifiUi.line1) * kFontW) / 2;
        if (x < 0) x = 0;
        gui.drawText(x, startY, msg.wifiUi.line1, ColorBlack, ColorWhite);
    }
    if (hasLine2) {
        int x = (kScreenW - utf8Count(msg.wifiUi.line2) * kFontW) / 2;
        if (x < 0) x = 0;
        gui.drawText(x, startY + kFontH + kLineGap, msg.wifiUi.line2, ColorBlack, ColorWhite);
    }
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

    // GPIO4 电池 ADC：配置 11dB 衰减（量程 ~0~3.1V），必须在主核心初始化
    analogSetPinAttenuation(4, ADC_11db);

    // 创建后台任务（各任务负责自身外设初始化，失败时通过消息队列上报）
    xTaskCreate(rtcTask,      "rtcTask",   2048, nullptr, 2, nullptr);
    xTaskCreate(humitureTask, "humTask",   2048, nullptr, 1, nullptr);
    xTaskCreate(wifiTask,     "wifiTask",  8192, nullptr, 2, nullptr);
    xTaskCreate(batteryTask,  "battTask",  4096, nullptr, 1, nullptr);
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
        // WiFi/NTP 流程期间抑制时钟重绘，避免时钟闪入 WiFi 文案区域
        if (!g_wifiUiVisible)
            handleRtcUpdate(msg.rtcTime);
        break;
    case MSG_HUMITURE_UPDATE:
        handleHumiture(msg.humiture.temp, msg.humiture.hum);
        break;
    case MSG_WIFI_STATUS:
        if (!msg.wifi.connected) Serial.println("WiFi 连接断开");
        // WiFi init 阶段结束（连接成功但 NTP 失败、或 WiFi 完全失败），归零标志让时钟可重绘
        // 若 NTP 同步成功，MSG_NTP_SYNC 先于本消息处理时已归零，此处 guard 避免重复 resetClockState
        if (g_wifiUiVisible) {
            g_wifiUiVisible = false;
            resetClockState();
        }
        break;
    case MSG_WIFI_UI:
        handleWifiUi(msg);
        break;
    case MSG_NTP_SYNC: {
        // NTP 同步成功，将时间写入 RTC
        rtc.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                    msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                    msg.ntpTime.weekday);
        Serial.println("时间已写入 RTC");
        // 归零 WiFi UI 标志，立即清屏并刷新时钟，不等下次分钟变化
        g_wifiUiVisible = false;
        gui.clear();
        RTCTime t;
        t.year    = msg.ntpTime.year;
        t.month   = msg.ntpTime.month;
        t.day     = msg.ntpTime.day;
        t.hour    = msg.ntpTime.hour;
        t.minute  = msg.ntpTime.minute;
        t.second  = msg.ntpTime.second;
        t.weekday = msg.ntpTime.weekday;
        resetClockState();
        handleRtcUpdate(t);
        break;
    }
    case MSG_BATTERY_UPDATE:
        Serial.printf("电池电压: %.2f V\n", msg.battery.voltage);
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
