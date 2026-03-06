//
// Created by Kelly on 26-2-12.
//

#include "Gui.h"
#include "fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h"
#include "effects/GlyphEffect.h"

#include <stdlib.h>
#include <stdint.h>

// ========================== 内部帮助函数 ==========================

namespace {

// UTF-8 解码，返回下一个 Unicode 码点，失败时返回 false 并跳过当前字节。
bool DecodeNextUTF8(const char *&p, uint32_t &codepoint) {
    if (p == nullptr || *p == '\0') {
        return false;
    }

    const uint8_t *s = reinterpret_cast<const uint8_t *>(p);
    uint8_t c = *s;

    if (c < 0x80) {
        // 单字节 ASCII
        codepoint = c;
        ++p;
        return true;
    }

    int extraBytes = 0;
    if ((c & 0xE0) == 0xC0) {
        extraBytes = 1;
        codepoint = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extraBytes = 2;
        codepoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extraBytes = 3;
        codepoint = c & 0x07;
    } else {
        // 非法起始字节，跳过
        ++p;
        return false;
    }

    ++s;
    for (int i = 0; i < extraBytes; ++i) {
        uint8_t cc = *s;
        if ((cc & 0xC0) != 0x80) {
            // 非法续字节，尽量回退到下一个起始位置
            p++;
            return false;
        }
        codepoint = (codepoint << 6) | (cc & 0x3F);
        ++s;
    }

    p = reinterpret_cast<const char *>(s);
    return true;
}

static void SwapInt(int &a, int &b) {
    int tmp = a;
    a = b;
    b = tmp;
}

}  // namespace

// =============================== Gui 实现 ===============================

Gui::Gui(DisplayPort *lcd, int width, int height)
    : lcd_(lcd),
      width_(width),
      height_(height),
      fgColor_(ColorBlack),
      bgColor_(ColorWhite),
      chineseGlyphProvider_(nullptr),
      currentFont_(&kFont_Alibaba72x96),
      bigDigitEffect_(BigDigitEffectParamsDefault()) {}

void Gui::clear(uint8_t color) {
    if (!lcd_) {
        return;
    }
    lcd_->RLCD_ColorClear(color);
}

void Gui::clear() {
    clear(bgColor_);
}

void Gui::display() {
    if (!lcd_) {
        return;
    }
    lcd_->RLCD_Display();
}

void Gui::setForegroundColor(uint8_t color) {
    fgColor_ = color;
}

void Gui::setBackgroundColor(uint8_t color) {
    bgColor_ = color;
}

uint8_t Gui::foregroundColor() const {
    return fgColor_;
}

uint8_t Gui::backgroundColor() const {
    return bgColor_;
}

void Gui::drawPixel(int x, int y, uint8_t color) {
    if (!lcd_) {
        return;
    }
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }
    lcd_->RLCD_SetPixel(static_cast<uint16_t>(x), static_cast<uint16_t>(y), color);
}

void Gui::drawLine(int x0, int y0, int x1, int y1, uint8_t color) {
    // Bresenham 直线算法，支持任意方向斜率
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Gui::drawRect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    // 顶边和底边
    drawLine(x, y, x + w - 1, y, color);
    if (h > 1) {
        drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    }

    // 左右边（避免与顶/底边重复绘制角点）
    if (h > 2) {
        drawLine(x, y + 1, x, y + h - 2, color);
        if (w > 1) {
            drawLine(x + w - 1, y + 1, x + w - 1, y + h - 2, color);
        }
    }
}

void Gui::fillRect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    for (int j = 0; j < h; ++j) {
        drawLine(x, y + j, x + w - 1, y + j, color);
    }
}

void Gui::drawCircle(int x0, int y0, int radius, uint8_t color) {
    if (radius <= 0) {
        return;
    }

    int x = radius;
    int y = 0;
    int err = 1 - radius;

    while (x >= y) {
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 + x, y0 - y, color);

        ++y;
        if (err < 0) {
            err += (y << 1) + 1;
        } else {
            --x;
            err += ((y - x) << 1) + 1;
        }
    }
}

void Gui::fillCircle(int x0, int y0, int radius, uint8_t color) {
    if (radius <= 0) {
        return;
    }

    // 以水平扫描线填充圆内区域
    int x = radius;
    int y = 0;
    int err = 1 - radius;

    while (x >= y) {
        // 利用对称性画 4 条水平线
        drawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        if (y != 0) {
            drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        }

        if (x != y) {
            drawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
            if (x != 0) {
                drawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
            }
        }

        ++y;
        if (err < 0) {
            err += (y << 1) + 1;
        } else {
            --x;
            err += ((y - x) << 1) + 1;
        }
    }
}

void Gui::drawRoundRect(int x, int y, int w, int h, int radius, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (radius <= 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    if (radius * 2 > w) {
        radius = w / 2;
    }
    if (radius * 2 > h) {
        radius = h / 2;
    }

    int x1 = x + w - 1;
    int y1 = y + h - 1;

    // 四条直线边（与圆角在半径处衔接）
    drawLine(x + radius, y, x1 - radius, y, color);
    drawLine(x + radius, y1, x1 - radius, y1, color);
    drawLine(x, y + radius, x, y1 - radius, color);
    drawLine(x1, y + radius, x1, y1 - radius, color);

    // 四角圆弧：圆心分别为 (x+r,y+r), (x1-r,y+r), (x+r,y1-r), (x1-r,y1-r)，半径 r
    int r = radius;
    int dx = r;
    int dy = 0;
    int err = 1 - r;
    while (dx >= dy) {
        // 左上 (cx-dx,cy-dy), (cx-dy,cy-dx)
        drawPixel(x + r - dx, y + r - dy, color);
        drawPixel(x + r - dy, y + r - dx, color);
        // 右上 (cx+dx,cy-dy), (cx+dy,cy-dx)
        drawPixel(x1 - r + dx, y + r - dy, color);
        drawPixel(x1 - r + dy, y + r - dx, color);
        // 左下 (cx-dx,cy+dy), (cx-dy,cy+dx)
        drawPixel(x + r - dx, y1 - r + dy, color);
        drawPixel(x + r - dy, y1 - r + dx, color);
        // 右下 (cx+dx,cy+dy), (cx+dy,cy+dx)
        drawPixel(x1 - r + dx, y1 - r + dy, color);
        drawPixel(x1 - r + dy, y1 - r + dx, color);

        ++dy;
        if (err < 0) {
            err += (dy << 1) + 1;
        } else {
            --dx;
            err += ((dy - dx) << 1) + 1;
        }
    }
}

void Gui::fillRoundRect(int x, int y, int w, int h, int radius, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (radius <= 0) {
        fillRect(x, y, w, h, color);
        return;
    }
    if (radius * 2 > w) {
        radius = w / 2;
    }
    if (radius * 2 > h) {
        radius = h / 2;
    }

    // 左/右圆角圆心的 x 坐标
    int cx_left  = x + radius;
    int cx_right = x + w - 1 - radius;

    // 中间矩形带（圆角高度以外的区域，全宽）
    fillRect(x, y + radius, w, h - 2 * radius, color);

    // 扫描线填充上下圆角区域：
    // 对 dy = 0..radius-1，计算圆弧在该行的水平跨度 horiz，
    // 然后画满从左弧到右弧的整行。
    for (int dy = 0; dy < radius; ++dy) {
        // vert：当前行到圆心行的垂直距离
        int vert = radius - dy;
        // horiz：满足 horiz^2 + vert^2 <= radius^2 的最大整数
        int horiz = 0;
        while ((horiz + 1) * (horiz + 1) + vert * vert <= radius * radius) {
            ++horiz;
        }
        // 上圆角行：y + dy，从 (cx_left - horiz) 到 (cx_right + horiz)
        drawLine(cx_left - horiz, y + dy, cx_right + horiz, y + dy, color);
        // 下圆角行：y + h - 1 - dy
        drawLine(cx_left - horiz, y + h - 1 - dy, cx_right + horiz, y + h - 1 - dy, color);
    }
}

void Gui::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void Gui::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
    // 按 y 坐标排序三个顶点：y0 <= y1 <= y2
    if (y0 > y1) {
        SwapInt(y0, y1);
        SwapInt(x0, x1);
    }
    if (y1 > y2) {
        SwapInt(y1, y2);
        SwapInt(x1, x2);
    }
    if (y0 > y1) {
        SwapInt(y0, y1);
        SwapInt(x0, x1);
    }

    if (y0 == y2) {
        // 三点几乎在同一水平线上，退化为一条线
        int minX = x0;
        int maxX = x0;
        if (x1 < minX) minX = x1;
        if (x2 < minX) minX = x2;
        if (x1 > maxX) maxX = x1;
        if (x2 > maxX) maxX = x2;
        drawLine(minX, y0, maxX, y0, color);
        return;
    }

    auto drawFlatTriangle = [&](int xa, int ya, int xb, int yb, int xc, int yc) {
        // ya == yb 或 yb == yc 的情形：顶部或底部为水平边
        if (yb == yc) {
            // xa,ya 为顶点；xb,yb 与 xc,yc 在底边
            float invAB = (yb == ya) ? 0.0f : static_cast<float>(xb - xa) / static_cast<float>(yb - ya);
            float invAC = (yc == ya) ? 0.0f : static_cast<float>(xc - xa) / static_cast<float>(yc - ya);
            float curx1 = static_cast<float>(xa);
            float curx2 = static_cast<float>(xa);
            for (int y = ya; y <= yb; ++y) {
                int xStart = static_cast<int>(curx1 + 0.5f);
                int xEnd = static_cast<int>(curx2 + 0.5f);
                if (xStart > xEnd) {
                    int t = xStart;
                    xStart = xEnd;
                    xEnd = t;
                }
                drawLine(xStart, y, xEnd, y, color);
                curx1 += invAB;
                curx2 += invAC;
            }
        } else {
            // xb,yb 为顶点；xa,ya 与 xc,yc 在顶边
            float invBA = (ya == yb) ? 0.0f : static_cast<float>(xa - xb) / static_cast<float>(ya - yb);
            float invBC = (yc == yb) ? 0.0f : static_cast<float>(xc - xb) / static_cast<float>(yc - yb);
            float curx1 = static_cast<float>(xb);
            float curx2 = static_cast<float>(xb);
            for (int y = yb; y <= yc; ++y) {
                int xStart = static_cast<int>(curx1 + 0.5f);
                int xEnd = static_cast<int>(curx2 + 0.5f);
                if (xStart > xEnd) {
                    int t = xStart;
                    xStart = xEnd;
                    xEnd = t;
                }
                drawLine(xStart, y, xEnd, y, color);
                curx1 += invBA;
                curx2 += invBC;
            }
        }
    };

    if (y1 == y2) {
        // 平底三角形
        drawFlatTriangle(x0, y0, x1, y1, x2, y2);
    } else if (y0 == y1) {
        // 平顶三角形
        drawFlatTriangle(x2, y2, x0, y0, x1, y1);
    } else {
        // 一般情况：通过在 v0-v2 上插值拆成平顶 + 平底两个三角形
        int x3 = x0 + static_cast<int>((static_cast<float>(y1 - y0) / static_cast<float>(y2 - y0)) * (x2 - x0));
        int y3 = y1;
        drawFlatTriangle(x0, y0, x1, y1, x3, y3);
        drawFlatTriangle(x2, y2, x1, y1, x3, y3);
    }
}

void Gui::setFont(const Font *font) {
    if (font) {
        currentFont_ = font;
    }
}

const Font *Gui::currentFont() const {
    return currentFont_;
}

// ── 核心渲染实现 ─────────────────────────────────────────────────────────────

void Gui::drawTextImpl(int x, int y, const char *utf8, const Font *font,
                       uint8_t fgColor, uint8_t bgColor) {
    if (!utf8 || !font) {
        return;
    }

    int cursorX = x;
    int cursorY = y;
    int lineHeight = font->lineHeight > 0 ? font->lineHeight : 8;

    const char *p = utf8;
    uint32_t codepoint = 0;
    while (DecodeNextUTF8(p, codepoint)) {
        if (codepoint == '\n') {
            cursorX = x;
            cursorY += lineHeight;
            continue;
        }

        // 遍历 fallback 链查找字形
        int w = 0, h = 0, stride = 0, advanceX = 0;
        const uint8_t *glyph = nullptr;
        for (const Font *f = font; f != nullptr; f = f->fallback) {
            if (f->getGlyph) {
                glyph = f->getGlyph(codepoint, w, h, stride, advanceX, f->data);
                if (glyph && w > 0 && h > 0 && stride > 0) {
                    break;
                }
            }
        }

        if (glyph && w > 0 && h > 0 && stride > 0) {
            if (cursorX + advanceX > width_) {
                cursorX = x;
                cursorY += lineHeight;
            }
            fillRect(cursorX, cursorY, advanceX, h, bgColor);
            for (int j = 0; j < h; ++j) {
                for (int i = 0; i < w; ++i) {
                    int byteIndex = j * stride + (i >> 3);
                    int bitIndex = 7 - (i & 0x7);
                    if (glyph[byteIndex] & (1U << bitIndex)) {
                        drawPixel(cursorX + i, cursorY + j, fgColor);
                    }
                }
            }
            cursorX += advanceX;
        } else {
            // 所有字体均无此字形：画占位符
            const int size = 16;
            if (cursorX + size > width_) {
                cursorX = x;
                cursorY += lineHeight;
            }
            fillRect(cursorX, cursorY, size, size, bgColor);
            drawRect(cursorX, cursorY, size, size, fgColor);
            drawLine(cursorX, cursorY, cursorX + size - 1, cursorY + size - 1, fgColor);
            drawLine(cursorX + size - 1, cursorY, cursorX, cursorY + size - 1, fgColor);
            cursorX += size;
        }
    }
}

void Gui::drawText(int x, int y, const char *utf8) {
    drawTextImpl(x, y, utf8, currentFont_, fgColor_, bgColor_);
}

void Gui::drawText(int x, int y, const char *utf8, uint8_t fgColor, uint8_t bgColor) {
    drawTextImpl(x, y, utf8, currentFont_, fgColor, bgColor);
}

void Gui::drawBitmap(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color) {
    if (!bitmap || w <= 0 || h <= 0) {
        return;
    }

    // 假设 bitmap 为 1bpp，按行从左到右，每字节 8 像素，高位在左
    int byteWidth = (w + 7) / 8;

    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int byteIndex = j * byteWidth + (i >> 3);
            int bitIndex = 7 - (i & 0x7);
            uint8_t b = bitmap[byteIndex];
            if (b & (1U << bitIndex)) {
                drawPixel(x + i, y + j, color);
            }
        }
    }
}

void Gui::setChineseGlyphProvider(ChineseGlyphProvider provider) {
    chineseGlyphProvider_ = provider;
}

void Gui::setBigDigitEffectParams(const BigDigitEffectParams &params) {
    bigDigitEffect_ = params;
}

void Gui::setBigDigitEffectParams(int boldLevel, int outlineWidth) {
    bigDigitEffect_.boldLevel = boldLevel;
    bigDigitEffect_.outlineWidth = outlineWidth;
}

const BigDigitEffectParams &Gui::bigDigitEffectParams() const {
    return bigDigitEffect_;
}

