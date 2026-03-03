#include "GuiTests.h"
#include <display_bsp.h>

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
    gui.drawString(10, 140, "FG only", ColorBlack);
    gui.drawString(10, 160, "FG/BG text");
    gui.drawString(10, 180, "Explicit", ColorBlack, ColorWhite);
}

void GuiTests::testUTF8Text(Gui &gui) {
    gui.drawUTF8String(10, 200, "Hello \xE4\xB8\x96\xE7\x95\x8C", ColorBlack);
    gui.drawUTF8String(10, 220, "UTF8 \xE4\xB8\xAD\xE6\x96\x87", ColorBlack, ColorWhite);
    
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
