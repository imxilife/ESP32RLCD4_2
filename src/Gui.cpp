//
// Created by Kelly on 26-2-12.
//

#include "Gui.h"

#include <stdlib.h>
#include <stdint.h>

// ========================== 内部帮助函数与字体定义 ==========================

namespace {

// 简单 5x7 数字字体，仅对 '0'~'9' 提供精细点阵。
// 其它 ASCII 字符会使用退化的占位绘制，但不会静默丢失。
static const uint8_t kFont5x7Digits[][5] = {
    // '0'
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    // '1'
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    // '2'
    {0x42, 0x61, 0x51, 0x49, 0x46},
    // '3'
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    // '4'
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    // '5'
    {0x27, 0x45, 0x45, 0x45, 0x39},
    // '6'
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    // '7'
    {0x01, 0x71, 0x09, 0x05, 0x03},
    // '8'
    {0x36, 0x49, 0x49, 0x49, 0x36},
    // '9'
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

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
      chineseGlyphProvider_(nullptr) {}

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

    // 中间的直线部分
    drawLine(x + radius, y, x1 - radius, y, color);
    drawLine(x + radius, y1, x1 - radius, y1, color);
    drawLine(x, y + radius, x, y1 - radius, color);
    drawLine(x1, y + radius, x1, y1 - radius, color);

    // 四个圆角，使用圆的八分之一点集
    int r = radius;
    int dx = r;
    int dy = 0;
    int err = 1 - r;
    while (dx >= dy) {
        // 左上
        drawPixel(x + radius - 1 - dy, y + radius - 1 - dx, color);
        drawPixel(x + radius - 1 - dx, y + radius - 1 - dy, color);
        // 右上
        drawPixel(x1 - radius + 1 + dy, y + radius - 1 - dx, color);
        drawPixel(x1 - radius + 1 + dx, y + radius - 1 - dy, color);
        // 左下
        drawPixel(x + radius - 1 - dx, y1 - radius + 1 + dy, color);
        drawPixel(x + radius - 1 - dy, y1 - radius + 1 + dx, color);
        // 右下
        drawPixel(x1 - radius + 1 + dx, y1 - radius + 1 + dy, color);
        drawPixel(x1 - radius + 1 + dy, y1 - radius + 1 + dx, color);

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

    int x1 = x + w - 1;
    int y1 = y + h - 1;

    // 先填充中间的矩形部分
    fillRect(x + radius, y, w - 2 * radius, h, color);

    // 使用水平扫描线填充四个圆角
    int r = radius;
    int dx = r;
    int dy = 0;
    int err = 1 - r;
    while (dx >= dy) {
        // 顶部两侧
        int topY1 = y + radius - 1 - dx;
        int topY2 = y + radius - 1 - dy;
        int leftX1 = x + radius - dx;
        int leftX2 = x + radius - dy;
        int rightX1 = x1 - radius + dx;
        int rightX2 = x1 - radius + dy;

        drawLine(leftX1, topY1, rightX1, topY1, color);
        if (dx != dy) {
            drawLine(leftX2, topY2, rightX2, topY2, color);
        }

        // 底部两侧
        int bottomY1 = y1 - radius + 1 + dx;
        int bottomY2 = y1 - radius + 1 + dy;
        drawLine(leftX1, bottomY1, rightX1, bottomY1, color);
        if (dx != dy) {
            drawLine(leftX2, bottomY2, rightX2, bottomY2, color);
        }

        ++dy;
        if (err < 0) {
            err += (dy << 1) + 1;
        } else {
            --dx;
            err += ((dy - dx) << 1) + 1;
        }
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

void Gui::drawChar(int x, int y, char c, uint8_t color) {
    // 可打印 ASCII 范围内的字符统一处理，不可打印字符画成小方框
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
        // 占位方框：3x7
        drawRect(x, y, 3, 7, color);
        return;
    }

    if (c >= '0' && c <= '9') {
        // 对数字使用精细 5x7 点阵
        const uint8_t *bitmap = kFont5x7Digits[c - '0'];
        for (int col = 0; col < 5; ++col) {
            uint8_t line = bitmap[col];
            for (int row = 0; row < 7; ++row) {
                if (line & (1U << row)) {
                    drawPixel(x + col, y + row, color);
                }
            }
        }
        return;
    }

    // 其它 ASCII 字符：使用基于字符编码的简易占位花纹，保证可见且区分度略高
    unsigned char uc = static_cast<unsigned char>(c);
    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            // 简单 hash：根据字符编码的比特模式决定是否点亮
            uint8_t mask = 1U << ((col + row) & 0x7);
            if (uc & mask) {
                drawPixel(x + col, y + row, color);
            }
        }
    }
}

void Gui::drawChar(int x, int y, char c) {
    drawChar(x, y, c, fgColor_, bgColor_);
}

void Gui::drawChar(int x, int y, char c, uint8_t fgColor, uint8_t bgColor) {
    unsigned char uc = static_cast<unsigned char>(c);

    // 不可打印字符：3x7 小方框，先填充背景再画边框
    if (uc < 0x20 || uc > 0x7E) {
        fillRect(x, y, 3, 7, bgColor);
        drawRect(x, y, 3, 7, fgColor);
        return;
    }

    if (c >= '0' && c <= '9') {
        // 数字使用精细 5x7 点阵：前景/背景都写入，覆盖字符单元
        const uint8_t *bitmap = kFont5x7Digits[c - '0'];
        for (int col = 0; col < 5; ++col) {
            uint8_t line = bitmap[col];
            for (int row = 0; row < 7; ++row) {
                bool on = (line & (1U << row)) != 0;
                drawPixel(x + col, y + row, on ? fgColor : bgColor);
            }
        }
        return;
    }

    // 其它可打印 ASCII：先填充一个 5x7 背景块，再按原 hash 规则画前景
    fillRect(x, y, 5, 7, bgColor);
    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            uint8_t mask = 1U << ((col + row) & 0x7);
            if (uc & mask) {
                drawPixel(x + col, y + row, fgColor);
            }
        }
    }
}

void Gui::drawString(int x, int y, const char *str, uint8_t color) {
    if (!str) {
        return;
    }
    int cursorX = x;
    while (*str) {
        drawChar(cursorX, y, *str, color);
        // 字符宽度固定为 6 像素（5 像素点阵 + 1 像素间隔）
        cursorX += 6;
        ++str;
    }
}

void Gui::drawString(int x, int y, const char *str) {
    drawString(x, y, str, fgColor_, bgColor_);
}

void Gui::drawString(int x, int y, const char *str, uint8_t fgColor, uint8_t bgColor) {
    if (!str) {
        return;
    }
    int cursorX = x;
    while (*str) {
        drawChar(cursorX, y, *str, fgColor, bgColor);
        cursorX += 6;
        ++str;
    }
}

void Gui::drawUTF8String(int x, int y, const char *utf8, uint8_t color) {
    if (!utf8) {
        return;
    }

    int cursorX = x;
    const char *p = utf8;
    uint32_t codepoint = 0;
    while (DecodeNextUTF8(p, codepoint)) {
        if (codepoint < 0x80) {
            // ASCII，复用 drawChar
            drawChar(cursorX, y, static_cast<char>(codepoint), color);
            cursorX += 6;
        } else {
            // 非 ASCII，按中文处理：优先查找外部点阵，否则画 16x16 占位符
            int w = 0;
            int h = 0;
            int stride = 0;
            const uint8_t *glyph = nullptr;
            if (chineseGlyphProvider_) {
                glyph = chineseGlyphProvider_(codepoint, w, h, stride);
            }

            if (glyph && w > 0 && h > 0 && stride > 0) {
                drawBitmap(cursorX, y, w, h, glyph, color);
                cursorX += w;
            } else {
                // 默认占位：16x16 方框 + 对角线
                const int size = 16;
                drawRect(cursorX, y, size, size, color);
                drawLine(cursorX, y, cursorX + size - 1, y + size - 1, color);
                drawLine(cursorX + size - 1, y, cursorX, y + size - 1, color);
                cursorX += size;
            }
        }
    }
}

void Gui::drawUTF8String(int x, int y, const char *utf8) {
    drawUTF8String(x, y, utf8, fgColor_, bgColor_);
}

void Gui::drawUTF8String(int x, int y, const char *utf8, uint8_t fgColor, uint8_t bgColor) {
    if (!utf8) {
        return;
    }

    int cursorX = x;
    const char *p = utf8;
    uint32_t codepoint = 0;
    while (DecodeNextUTF8(p, codepoint)) {
        if (codepoint < 0x80) {
            // ASCII：使用前景/背景色版本，覆盖字符单元
            drawChar(cursorX, y, static_cast<char>(codepoint), fgColor, bgColor);
            cursorX += 6;
        } else {
            // 非 ASCII：按中文处理
            int w = 0;
            int h = 0;
            int stride = 0;
            const uint8_t *glyph = nullptr;
            if (chineseGlyphProvider_) {
                glyph = chineseGlyphProvider_(codepoint, w, h, stride);
            }

            if (glyph && w > 0 && h > 0 && stride > 0) {
                // 先填充背景，再按 stride 读点阵画前景
                fillRect(cursorX, y, w, h, bgColor);
                for (int j = 0; j < h; ++j) {
                    for (int i = 0; i < w; ++i) {
                        int byteIndex = j * stride + (i >> 3);
                        int bitIndex = 7 - (i & 0x7);
                        uint8_t b = glyph[byteIndex];
                        if (b & (1U << bitIndex)) {
                            drawPixel(cursorX + i, y + j, fgColor);
                        }
                    }
                }
                cursorX += w;
            } else {
                // 默认占位：16x16 方框 + 对角线，使用前景/背景色
                const int size = 16;
                fillRect(cursorX, y, size, size, bgColor);
                drawRect(cursorX, y, size, size, fgColor);
                drawLine(cursorX, y, cursorX + size - 1, y + size - 1, fgColor);
                drawLine(cursorX + size - 1, y, cursorX, y + size - 1, fgColor);
                cursorX += size;
            }
        }
    }
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

