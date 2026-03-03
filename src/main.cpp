#include <Arduino.h>
#include <display_bsp.h>
#include <Gui.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <GuiTests.h>
#include <WiFiConfig.h>

#define ENABLE_GUI_TESTS 0

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

// 全局 RTC 实例（PCF85063）
RTC85063 rtc;

// 全局温湿度传感器实例
Humiture humiture;

// WiFi 配置实例
WiFiConfig wifiConfig(&gui);

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
    gui.drawUTF8String(weekX, kWeekY, kWeekNames[wd], ColorBlack);
}

void setup() {

    Serial.begin(115200);

    RlcdPort.RLCD_Init();                 // 初始化 LCD
    // 配置单色前景/背景色
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);

    gui.clear();

#if ENABLE_GUI_TESTS
    GuiTests::runAllTests(gui);
    delay(5000);
    gui.clear();
#endif

    // ===================== WiFi 初始化 =====================
    wifiConfig.begin();

    // ===================== RTC 初始化 =====================
    // PCF85063 接线：SDA=GPIO13, SCL=GPIO14
    rtc.begin(13, 14);

    rtc.setTime(2026, 3, 2, 23, 34, 0, 1);
    delay(100);

    RTCTime now = rtc.now();
    rtc.setAlarm(10, 3, onAlarmTriggered);

    // ===================== 温湿度传感器初始化 =====================
    Serial.println("\n=== SHTC3 Temperature & Humidity Sensor Test ===");
    
    if (!humiture.begin()) {
        Serial.println("温湿度传感器初始化失败！");
        while (1) delay(10);
    }
}

void loop() {
    RTCTime now = rtc.now();
    rtc.alarmListener();

    // 时间：每分钟刷新一次
    if (now.minute != s_lastMinute) {
        drawTime(now);
        s_lastMinute = now.minute;
    }

    // 日期/星期：每天刷新一次（也在首次进入时刷新）
    if (now.day != s_lastDay) {
        drawDateWeek(now);
        s_lastDay = now.day;
    }

    // ===================== 读取温湿度 =====================
    float temperature, humidity;
    if (humiture.read(temperature, humidity)) {
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

        gui.display();
    }

    // ===================== WiFi 状态监控 =====================
    // 检查 WiFi 连接状态
    if (!wifiConfig.isConnected()) {
        Serial.println("WiFi 连接断开");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}