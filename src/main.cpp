#include <Arduino.h>
#include <display_bsp.h>
#include <Gui.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <GuiTests.h>
#include <WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "app_message.h"
#include "fonts/pkBitmap.h"
#include "fonts/Font5x7.h"
#include "fonts/FontDigits.h"
#include "fonts/chinese_font.h"
#include "fonts/Font96_AlibabaPuHuiTi_3_75_SemiBold.h"

#define ENABLE_GUI_TESTS 0

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

// 全局 RTC 实例（PCF85063）
RTC85063 rtc;

// 全局温湿度传感器实例
Humiture humiture;

// WiFi 配置实例（不再直接持有 GUI）
WiFiConfig wifiConfig;

// 应用消息队列（主 loop 作为 UI 事件循环）
static QueueHandle_t g_msgQueue = nullptr;

// 触摸与按键引脚（占位，按实际硬件修改）
static constexpr int kTouchIntPin  = -1; // TODO: 设置为真实触摸中断引脚
static constexpr int kButtonIntPin = -1; // TODO: 设置为真实按键中断引脚

// 闹钟回调函数
void onAlarmTriggered() {
    Serial.println(">>> Alarm callback executed! <<<");
}

// ── 时间显示参数（居中，大号 72x96）──────────────────────────
static const int kTimeCharW  = 72;
static const int kTimeCharH  = 96;
static const int kTimeStrLen = 5;   // "HH:MM"
static const int kTimeX = (400 - kTimeStrLen * kTimeCharW) / 2;
static const int kTimeY = (300 - kTimeCharH) / 2;

// ── 日期/星期显示参数（右上角，小号 24x32）────────────────────
// 日期 "03/02"：5 字符 × 24px = 120px
// 星期 "星期三"：3 汉字 × 16px = 48px（中文字库 16x16）
// 两行竖排，顶部留 10px 边距，右边留 10px
static const int kDateY    = 8;           // 日期行 Y
static const int kWeekY    = kDateY + 32 + 4;  // 星期行 Y（日期高32，间距4）
static const int kDateRightMargin = 10;   // 距屏幕右边距

// 星期名称对应的 UTF-8 编码（星期 + 一/二/三/四/五/六/日）
static const char *kWeekNames[7] = {
    "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5",  // 星期日 (weekday=0)
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",  // 星期一
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c",  // 星期二
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89",  // 星期三
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b",  // 星期四
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",  // 星期五
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad",  // 星期六
};

static uint8_t s_lastMinute = 0xFF;
static uint8_t s_lastDay    = 0xFF;

// ── RTOS Task 前置声明 ─────────────────────────────────────────────
void rtcTask(void* pvParameters);
void humitureTask(void* pvParameters);
void wifiTask(void* pvParameters);

// ── 中断服务函数前置声明 ─────────────────────────────────────────
void IRAM_ATTR onTouchISR();
void IRAM_ATTR onButtonISR();
void wifiUiMessageHandler(const char* line1, const char* line2);

// 绘制时间
void drawTime(const RTCTime &t) {
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.hour, t.minute);
    gui.fillRect(kTimeX, kTimeY, kTimeStrLen * kTimeCharW, kTimeCharH, ColorWhite);
    gui.drawBigDigits(kTimeX, kTimeY, timeBuf, ColorBlack);
}

// 绘制日期和星期
void drawDateWeek(const RTCTime &t) {
    // 日期字符串 "MM/DD"，用 drawSmallDigits（24x32）
    char dateBuf[8];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", t.month, t.day);

    // 日期总宽 = 5字符 × 24px = 120px，右对齐
    int dateW = 5 * 24;
    int dateX = 400 - dateW - kDateRightMargin;

    // 星期总宽 = 3汉字 × 16px = 48px，右对齐
    int weekW = 3 * 16;
    int weekX = 400 - weekW - kDateRightMargin;

    // 清除日期/星期区域（取两行中较宽的）
    int clearW = (dateW > weekW) ? dateW : weekW;
    int clearX = 400 - clearW - kDateRightMargin;
    int clearH = 32 + 4 + 16 + 4;  // 日期行高 + 间距 + 星期行高 + 下边距
    gui.fillRect(clearX, kDateY - 2, clearW + 2, clearH, ColorWhite);

    // 绘制日期
    gui.drawSmallDigits(dateX, kDateY, dateBuf, ColorBlack);

    // 绘制星期（中文，16x16）
    uint8_t wd = t.weekday < 7 ? t.weekday : 0;
    //gui.drawUTF8String(weekX, kWeekY, kWeekNames[wd], ColorBlack);
}

// ── 字模综合测试 ──────────────────────────────────────────────
// 在屏幕上排列展示所有内置字体，验证新 drawText/setFont API
void testAllFonts() {
    gui.clear(ColorWhite);

    int y = 4;

    // 1. kFont_ASCII5x7 — 5x7 ASCII，行高 8
    gui.drawText(4, y, "ASCII 5x7: Hello 123!@#", &kFont_ASCII5x7, ColorBlack);
    y += 12;

    // 2. kFont_Mixed — 中英混排：中文 16x16，回退 ASCII 5x7
    gui.drawText(4, y,
        "中文Hello 中文123",
        &kFont_Mixed, ColorBlack);   // 中文 Hello 温度 123
    y += 20;

    // 3. kFont_Chinese16x16 单独测试（找不到字时画占位符）
    gui.drawText(4, y,
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89"   // 星期三
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",   // 星期五
        &kFont_Chinese16x16, ColorBlack);
    y += 20;

    // 4. setFont 风格（u8g2 风格：设置一次，多次调用）
    gui.setFont(&kFont_Mixed);
    gui.drawText(4, y, "\xe6\xb8\xa9\xe5\xba\xa6: 25.3C  \xe6\xb9\xbf\xe5\xba\xa6: 60%");
    y += 20;    // 温度: 25.3C  湿度: 60%

    // 5. kFont_SmallDigit — 24x32 小号数字
    gui.drawText(4, y, "03/05", &kFont_SmallDigit, ColorBlack);
    y += 36;

    // 6. kFont_BigDigit — 72x96 大号数字（时间格式）
    gui.drawText(4, y, "12:34", &kFont_BigDigit, ColorBlack);
    y += 100;

    // 7. kFont_Alibaba72x96 — 阿里巴巴字体数字
    gui.drawText(4, y, "56:78", &kFont_Alibaba72x96, ColorBlack);

    gui.display();
}

// ===================== RTOS Tasks =====================

void rtcTask(void* pvParameters) {
    (void)pvParameters;
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
        vTaskDelay(pdMS_TO_TICKS(30000)); // 每 30 秒读取一次
    }
}

void wifiTask(void* pvParameters) {
    (void)pvParameters;

    // 在专门的 WiFi Task 中执行阻塞式配网与 NTP，同步完成后再进入状态监控
    wifiConfig.begin(&rtc);

    bool lastConnected = wifiConfig.isConnected();

    // 上报一次当前状态
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

// ===================== 外部中断 ISR =====================

void IRAM_ATTR onTouchISR() {
    if (g_msgQueue == nullptr) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type    = MSG_TOUCH_EVENT;
    msg.touch.x = 0; // TODO: 根据触摸芯片读取真实坐标
    msg.touch.y = 0;

    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR onButtonISR() {
    if (g_msgQueue == nullptr) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type         = MSG_BUTTON_EVENT;
    msg.button.btnId = 0; // TODO: 区分不同按键

    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// WiFiConfig 文本提示回调：将其转换为 WiFi UI 消息投入队列
void wifiUiMessageHandler(const char* line1, const char* line2) {
    if (g_msgQueue == nullptr) {
        return;
    }

    AppMessage msg;
    msg.type = MSG_WIFI_UI;

    // 安全拷贝字符串到固定大小缓冲区
    memset(msg.wifiUi.line1, 0, WIFI_UI_LINE_MAX);
    memset(msg.wifiUi.line2, 0, WIFI_UI_LINE_MAX);
    if (line1 != nullptr) {
        strncpy(msg.wifiUi.line1, line1, WIFI_UI_LINE_MAX - 1);
    }
    if (line2 != nullptr) {
        strncpy(msg.wifiUi.line2, line2, WIFI_UI_LINE_MAX - 1);
    }

    xQueueSend(g_msgQueue, &msg, 0);
}

void setup() {

    Serial.begin(115200);

    RlcdPort.RLCD_Init();                 // 初始化 LCD
    // 配置单色前景/背景色
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);
    gui.clear();

    // 字模综合测试：展示所有内置字体
    testAllFonts();

#if ENABLE_GUI_TESTS
    GuiTests::runAllTests(gui);
    delay(5000);
    gui.clear();
#endif

    // ===================== RTC 初始化 =====================
    // PCF85063 接线：SDA=GPIO13, SCL=GPIO14
#if ENABLE_GUI_TESTS
    rtc.begin(13, 14);

    // 设置一个默认时间（如果 NTP 同步失败会使用这个时间）
    rtc.setTime(2026, 01, 01, 0, 59, 59, 1);
    delay(100);

    // 预读一次当前时间，初始化显示
    RTCTime now = rtc.now();
    drawTime(now);
    drawDateWeek(now);
    s_lastMinute = now.minute;
    s_lastDay    = now.day;

    rtc.setAlarm(10, 3, onAlarmTriggered);

    // ===================== 温湿度传感器初始化 =====================
    Serial.println("\n=== SHTC3 Temperature & Humidity Sensor Test ===");
    
    if (!humiture.begin()) {
        Serial.println("温湿度传感器初始化失败！");
        while (1) delay(10);
    }

    // ===================== 创建消息队列 =====================
    g_msgQueue = xQueueCreate(16, sizeof(AppMessage));
    if (g_msgQueue == nullptr) {
        Serial.println("创建消息队列失败！系统无法继续运行");
        while (1) delay(100);
    }

    // 设置 WiFi 文本提示回调，将 WiFiConfig 内部状态提示转为队列消息
    wifiConfig.setMessageCallback(wifiUiMessageHandler);

    // ===================== 注册外部中断（如有实际引脚） =====================
    if (kTouchIntPin >= 0) {
        pinMode(kTouchIntPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kTouchIntPin), onTouchISR, FALLING);
    }
    if (kButtonIntPin >= 0) {
        pinMode(kButtonIntPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kButtonIntPin), onButtonISR, FALLING);
    }

    // ===================== 创建后台任务 =====================
    xTaskCreate(rtcTask, "rtcTask", 2048, nullptr, 2, nullptr);
    xTaskCreate(humitureTask, "humTask", 2048, nullptr, 1, nullptr);
    xTaskCreate(wifiTask, "wifiTask", 8192, nullptr, 2, nullptr);
#endif

}

// 主线程：作为 UI Looper，只处理消息并更新界面
void loop() {

#if ENABLE_GUI_TESTS
    if (g_msgQueue == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    AppMessage msg;
    // 等待任意模块投递的 UI 消息（带超时，便于未来空闲逻辑扩展）
    if (xQueueReceive(g_msgQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        switch (msg.type) {
        case MSG_RTC_UPDATE: {
            const RTCTime &t = msg.rtcTime;
            // 时间：每分钟刷新一次
            if (t.minute != s_lastMinute) {
                drawTime(t);
                s_lastMinute = t.minute;
            }
            // 日期/星期：每天刷新一次
            if (t.day != s_lastDay) {
                drawDateWeek(t);
                s_lastDay = t.day;
            }
            break;
        }
        case MSG_HUMITURE_UPDATE: {
            float temperature = msg.humiture.temp;
            float humidity    = msg.humiture.hum;
            Serial.printf("温度: %.1f °C  湿度: %.1f %%\n", temperature, humidity);

            // ===================== 在屏幕底部显示温湿度 =====================
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.1fC", temperature);

            char humidStr[16];
            snprintf(humidStr, sizeof(humidStr), "%.1f%%", humidity);

            int tempY  = 300 - 32 - 20;
            int humidY = tempY;
            int humidX = 400 - (int)strlen(humidStr) * 24 - 20;

            gui.fillRect(0, tempY - 5, 400, 32 + 10, ColorWhite);
            gui.drawSmallDigits(20, tempY, tempStr, ColorBlack);
            gui.drawSmallDigits(humidX, humidY, humidStr, ColorBlack);
            break;
        }
        case MSG_WIFI_STATUS: {
            if (!msg.wifi.connected) {
                Serial.println("WiFi 连接断开");
            }
            break;
        }
        case MSG_WIFI_UI: {
            // 在屏幕中部区域显示 WiFi 配网/同步过程的文本提示
            gui.fillRect(0, 100, 400, 100, ColorWhite);

            if (msg.wifiUi.line1[0] != '\0') {
                gui.drawUTF8String(10, 110, msg.wifiUi.line1, ColorBlack);
            }
            if (msg.wifiUi.line2[0] != '\0') {
                // 第二行通常是较短的英文/数字，可用小号字体
                gui.drawSmallDigits(10, 140, msg.wifiUi.line2, ColorBlack);
            }
            break;
        }
        case MSG_TOUCH_EVENT: {
            // 这里可以根据触摸坐标触发界面交互
            Serial.printf("触摸事件: x=%d, y=%d\n", msg.touch.x, msg.touch.y);
            break;
        }
        case MSG_BUTTON_EVENT: {
            Serial.printf("按键事件: id=%d\n", msg.button.btnId);
            break;
        }
        default:
            break;
        }

        // 统一在主线程刷屏，避免多任务并发操作 GUI
        gui.display();
    } else {
        // 超时分支：当前没有消息，可在此添加轻量级空闲逻辑（目前留空）
    }
#endif

}