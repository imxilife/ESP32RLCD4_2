#include "GuiTests.h"
#include <device/display/display_bsp.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h>

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
    gui.setFont(&kFont_Alibaba72x96);
    gui.drawText(10, 140, "12:34");
}

void GuiTests::testUTF8Text(Gui &gui) {
    gui.setFont(&kFont_Alibaba72x96);
    gui.drawText(10, 170, "56:78");
    gui.drawText(10, 210, "00:00", ColorBlack, ColorWhite);

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
