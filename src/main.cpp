#include <Arduino.h>
#include <Adafruit_SHTC3.h>
#include "display_bsp.h"
#include "Gui.h"
#include "RTC85063.h"


#define ENABLE 1
#define DISABLE 0

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

// 全局 RTC 实例（PCF85063）
RTC85063 rtc;

// 全局 SHTC3 实例
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();

// 闹钟回调函数
void onAlarmTriggered() {
    Serial.println(">>> Alarm callback executed! <<<");
}

void setup() {

    Serial.begin(115200);

    RlcdPort.RLCD_Init();                 // 初始化 LCD
    // 配置单色前景/背景色
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);

    // 1. 清屏为当前背景色
    gui.clear();  // 等价于 clear(ColorWhite)

    // 2. 基本几何图形测试
    // 矩形与实心矩形
#if DISABLE
    gui.drawRect(5, 5, 80, 40, gui.foregroundColor());
    gui.fillRect(10, 10, 60, 20, gui.foregroundColor());

    // 圆与实心圆
    gui.drawCircle(150, 40, 20, gui.foregroundColor());
    gui.fillCircle(220, 40, 15, gui.foregroundColor());
#endif

    // 圆角矩形
#if DISABLE
    gui.drawRoundRect(5, 60, 80, 40, 8, gui.foregroundColor());
    gui.fillRoundRect(100, 60, 80, 40, 10, gui.foregroundColor());
#endif


    // 三角形
#if DISABLE
    gui.drawTriangle(220, 60, 260, 100, 200, 100, gui.foregroundColor());
    gui.fillTriangle(300, 60, 340, 100, 260, 100, gui.foregroundColor());

    // 3. 线段测试（对角线、水平/垂直线）
    gui.drawLine(0, 0, 399, 299, gui.foregroundColor());
    gui.drawLine(0, 299, 399, 0, gui.foregroundColor());
    gui.drawLine(0, 150, 399, 150, gui.foregroundColor());
    gui.drawLine(200, 0, 200, 299, gui.foregroundColor());
#endif

    // 4. 文本测试：ASCII + UTF-8（含中文），前景/背景组合
    // 4.1 仅前景、透明背景
#if DISABLE
    gui.drawString(10, 140, "FG only", ColorBlack);

    // 4.2 使用当前前景/背景色，覆盖文本背景
    gui.drawString(10, 160, "FG/BG text");

    // 4.3 显式指定前景/背景
    gui.drawString(10, 180, "Explicit", ColorBlack, ColorWhite);
#endif

#if DISABLE
    // 4.4 UTF-8 字符串（含中文），仅前景
    gui.drawUTF8String(10, 200, "Hello \xE4\xB8\x96\xE7\x95\x8C", ColorBlack); // "Hello 世界"

    // 4.5 UTF-8 字符串（含中文），使用当前前景/背景覆盖背景
    gui.drawUTF8String(10, 220, "UTF8 \xE4\xB8\xAD\xE6\x96\x87", ColorBlack, ColorWhite); // "UTF8 中文"
#endif

    // 4.6 大号数字字体参数（72x96，Arial Black）
    gui.setBigDigitEffectParams(0, 0);

    // 5. 刷新到 LCD
    gui.display();

    // ===================== RTC 初始化 =====================
    // PCF85063 接线：SDA=GPIO13, SCL=GPIO14
    rtc.begin(13, 14);

    // 设置初始时间：2026/03/02 17:50:00 星期一
    rtc.setTime(2026, 3, 2, 23, 34, 0, 1);  // 1=星期一
    delay(100);  // 等待振荡器稳定

    // 读取并验证时间
    RTCTime now = rtc.now();

    // 设置闹钟：10 秒后触发，持续 3 秒
    rtc.setAlarm(10, 3, onAlarmTriggered);

    // ===================== SHTC3 初始化 =====================
    Serial.println("\n=== SHTC3 Temperature & Humidity Sensor Test ===");
    
    if (!shtc3.begin()) {
        Serial.println("SHTC3 初始化失败！请检查接线:");
        Serial.println("  - SDA: GPIO13");
        Serial.println("  - SCL: GPIO14");
        Serial.println("  - VCC: 3.3V");
        Serial.println("  - GND: GND");
        while (1) delay(10);
    }
    
    Serial.println("SHTC3 初始化成功！");
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

void drawTime(const RTCTime &t) {
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.hour, t.minute);
    gui.fillRect(kTimeX, kTimeY, kTimeStrLen * kTimeCharW, kTimeCharH, ColorWhite);
    gui.drawBigDigits(kTimeX, kTimeY, timeBuf, ColorBlack);
}

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

    // ===================== SHTC3 读取温湿度 =====================
    sensors_event_t humidity, temp;
    shtc3.getEvent(&humidity, &temp);

    Serial.printf("温度: %.1f °C  湿度: %.1f %%\n",
                  temp.temperature, humidity.relative_humidity);

    // ===================== 在屏幕底部显示温湿度 =====================
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1fC", temp.temperature);

    char humidStr[16];
    snprintf(humidStr, sizeof(humidStr), "%.1f%%", humidity.relative_humidity);

    int tempY  = 300 - 32 - 20;
    int humidY = tempY;
    int humidX = 400 - (int)strlen(humidStr) * 24 - 20;

    gui.fillRect(0, tempY - 5, 400, 32 + 10, ColorWhite);
    gui.drawSmallDigits(20, tempY, tempStr, ColorBlack);
    gui.drawSmallDigits(humidX, humidY, humidStr, ColorBlack);

    gui.display();

    vTaskDelay(pdMS_TO_TICKS(1000));
}