#include "TextView.h"
#include <Gui.h>

void TextView::draw(Gui& gui) {
    if (!visible || !text) return;

    if (bgColor != ColorTransparent) {
        gui.fillRect(x, y, width, height, bgColor);
    }

    if (font) {
        gui.setFont(font);
        gui.drawText(x, y, text, fgColor, bgColor);
    }
}
