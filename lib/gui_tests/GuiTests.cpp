#include "GuiTests.h"
#include <display_bsp.h>
#include "fonts/Font5x7.h"
#include "fonts/FontDigits.h"
#include "fonts/chinese_font.h"

void GuiTests::testBasicShapes(Gui &gui) {
    gui.drawRect(5, 5, 80, 40, gui.foregroundColor());
    gui.fillRect(10, 10, 60, 20, gui.foregroundColor());

    gui.drawCircle(150, 40, 20, gui.foregroundColor());
    gui.fillCircle(220, 40, 15, gui.foregroundColor());
}

void GuiTests::testRoundRect(Gui &gui) {
    gui.drawRoundRect(5, 60, 80, 40, 8, gui.foregroundColor());
    gui.fillRoundRect(100, 60, 80, 40, 10, gui.foregroundColor());
}

void GuiTests::testTriangleAndLines(Gui &gui) {
    gui.drawTriangle(220, 60, 260, 100, 200, 100, gui.foregroundColor());
    gui.fillTriangle(300, 60, 340, 100, 260, 100, gui.foregroundColor());

    gui.drawLine(0, 0, 399, 299, gui.foregroundColor());
    gui.drawLine(0, 299, 399, 0, gui.foregroundColor());
    gui.drawLine(0, 150, 399, 150, gui.foregroundColor());
    gui.drawLine(200, 0, 200, 299, gui.foregroundColor());
}

void GuiTests::testAsciiText(Gui &gui) {
    // 使用默认字体（kFont_Mixed）画 ASCII
    gui.drawText(10, 140, "Hello, World!", ColorBlack);
    // 显式指定 ASCII 5x7 字体
    gui.drawText(10, 152, "Font5x7: ABC 123", &kFont_ASCII5x7, ColorBlack);
}

void GuiTests::testUTF8Text(Gui &gui) {
    // 使用 kFont_Mixed：中文走 Chinese16x16，找不到时回退 ASCII 5x7
    gui.setFont(&kFont_Mixed);
    gui.drawText(10, 170, "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xB8\x89");  // 星期三
    gui.drawText(10, 190, "\xE4\xB8\xAD\xE6\x96\x87 Hello 123");       // 中文 Hello 123

    // 显式传 font 参数（中英混排）
    gui.drawText(10, 210, "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xBA\x94 Fri", &kFont_Mixed, ColorBlack);

    // 显式前/背景色
    gui.drawText(10, 230, "FG/BG text", &kFont_ASCII5x7, ColorBlack, ColorWhite);

    gui.setBigDigitEffectParams(0, 0);
    gui.display();
}

void GuiTests::runAllTests(Gui &gui) {
    gui.clear();
    
    testBasicShapes(gui);
    testRoundRect(gui);
    testTriangleAndLines(gui);
    testAsciiText(gui);
    testUTF8Text(gui);
    
    gui.display();
}
