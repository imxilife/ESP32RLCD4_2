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

    // 4.6 大号数字字体显示时间（72x96，Arial Black，无需额外加粗）
    gui.setBigDigitEffectParams(0, 0);  // 字体本身已是超粗，不需要 GlyphEffect
    gui.drawBigDigits((400 - 5 * 72) / 2, 30, "22:33", ColorBlack);

    // 4.7 小号数字字体显示数值（24x32，Arial Bold）
    gui.drawSmallDigits(20, 260, "22.9", ColorBlack);

    // 5. 刷新到 LCD
    gui.display();

    // ===================== RTC 初始化 =====================
    // PCF85063 接线：SDA=GPIO13, SCL=GPIO14
    rtc.begin(13, 14);

    // 设置初始时间：2026/03/02 17:50:00 星期一
    rtc.setTime(2026, 3, 2, 17, 50, 0, 1);  // 1=星期一
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

void loop() {
    // 读取并打印当前时间
    RTCTime now = rtc.now();
    
    // 闹钟监听器
    rtc.alarmListener();
    
    // ===================== SHTC3 读取温湿度 =====================
    sensors_event_t humidity, temp;
    
    // 读取传感器数据
    shtc3.getEvent(&humidity, &temp);
    
    // 打印温度数据（保留1位小数）
    Serial.print("温度: ");
    Serial.print(temp.temperature, 1);
    Serial.println(" °C");
    
    // 打印湿度数据（保留1位小数）
    Serial.print("湿度: ");
    Serial.print(humidity.relative_humidity, 1);
    Serial.println(" %");
    
    Serial.println("----------------------------");
    
    // 延迟 2 秒
    vTaskDelay(pdMS_TO_TICKS(2000));
}