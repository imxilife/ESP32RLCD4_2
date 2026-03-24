#pragma once

#include "View.h"
#include <Font.h>

class CardView : public View {
public:
    int cornerRadius = 8;
    const char* title = nullptr;
    const Font* titleFont = nullptr;

    void draw(Gui& gui) override;

    // 以 (cx, cy) 为中心，按 scale 缩放绘制
    void drawScaled(Gui& gui, int cx, int cy, float scale);
};
