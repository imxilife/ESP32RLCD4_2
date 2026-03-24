#include "CardView.h"
#include <Gui.h>
#include <cstring>
#include <algorithm>

void CardView::draw(Gui& gui) {
    if (!visible) return;
    drawScaled(gui, x + width / 2, y + height / 2, 1.0f);
}

void CardView::drawScaled(Gui& gui, int cx, int cy, float scale) {
    if (!visible) return;

    int sw = (int)(width * scale);
    int sh = (int)(height * scale);
    int sx = cx - sw / 2;
    int sy = cy - sh / 2;

    // 裁剪：完全在屏幕外则跳过
    if (sx + sw < 0 || sx >= 400 || sy + sh < 0 || sy >= 300) return;

    int sr = std::max(1, (int)(cornerRadius * scale));

    // 背景 + 边框
    gui.fillRoundRect(sx, sy, sw, sh, sr, bgColor);
    gui.drawRoundRect(sx, sy, sw, sh, sr, fgColor);

    // 居中绘制标题
    if (title && titleFont) {
        // 计算标题文字宽度（遍历 UTF-8 字符，累加 advanceX）
        int textW = 0;
        const char* p = title;
        while (*p) {
            uint32_t cp = 0;
            if ((*p & 0x80) == 0) {
                cp = *p++;
            } else if ((*p & 0xE0) == 0xC0) {
                cp = (*p & 0x1F) << 6; p++;
                cp |= (*p & 0x3F); p++;
            } else if ((*p & 0xF0) == 0xE0) {
                cp = (*p & 0x0F) << 12; p++;
                cp |= (*p & 0x3F) << 6; p++;
                cp |= (*p & 0x3F); p++;
            } else {
                p++; continue;
            }

            int gw, gh, stride, advance;
            const Font* f = titleFont;
            while (f) {
                if (f->getGlyph(cp, gw, gh, stride, advance, f->data)) {
                    textW += advance;
                    break;
                }
                f = f->fallback;
            }
        }

        int textH = titleFont->lineHeight;
        int tx = cx - textW / 2;
        int ty = cy - textH / 2;

        // 确保文字不超出卡片边界
        if (tx < sx + 2) tx = sx + 2;
        if (ty < sy + 2) ty = sy + 2;

        gui.setFont(titleFont);
        gui.drawText(tx, ty, title, fgColor, bgColor);
    }
}
