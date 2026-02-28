#include <Arduino.h>
#include "display_bsp.h"
#include "Gui.h"


DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui gui(&RlcdPort, 400, 300);

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
    gui.drawRect(5, 5, 80, 40, gui.foregroundColor());
    gui.fillRect(10, 10, 60, 20, gui.foregroundColor());

    // 圆与实心圆
    gui.drawCircle(150, 40, 20, gui.foregroundColor());
    gui.fillCircle(220, 40, 15, gui.foregroundColor());

    // 圆角矩形
    gui.drawRoundRect(5, 60, 80, 40, 8, gui.foregroundColor());
    gui.fillRoundRect(100, 60, 80, 40, 10, gui.foregroundColor());

    // 三角形
    gui.drawTriangle(220, 60, 260, 100, 200, 100, gui.foregroundColor());
    gui.fillTriangle(300, 60, 340, 100, 260, 100, gui.foregroundColor());

    // 3. 线段测试（对角线、水平/垂直线）
    gui.drawLine(0, 0, 399, 299, gui.foregroundColor());
    gui.drawLine(0, 299, 399, 0, gui.foregroundColor());
    gui.drawLine(0, 150, 399, 150, gui.foregroundColor());
    gui.drawLine(200, 0, 200, 299, gui.foregroundColor());

    // 4. 文本测试：ASCII + UTF-8（含中文），前景/背景组合
    // 4.1 仅前景、透明背景
    gui.drawString(10, 140, "FG only", ColorBlack);

    // 4.2 使用当前前景/背景色，覆盖文本背景
    gui.drawString(10, 160, "FG/BG text");

    // 4.3 显式指定前景/背景
    gui.drawString(10, 180, "Explicit", ColorBlack, ColorWhite);

    // 4.4 UTF-8 字符串（含中文），仅前景
    gui.drawUTF8String(10, 200, "Hello \xE4\xB8\x96\xE7\x95\x8C", ColorBlack); // "Hello 世界"

    // 4.5 UTF-8 字符串（含中文），使用当前前景/背景覆盖背景
    gui.drawUTF8String(10, 220, "UTF8 \xE4\xB8\xAD\xE6\x96\x87", ColorBlack, ColorWhite); // "UTF8 中文"

    // 5. 刷新到 LCD
    gui.display();
}

void loop() {
//
    //delay(1000);
    //Serial.printf("aaa");
    while(1);
}