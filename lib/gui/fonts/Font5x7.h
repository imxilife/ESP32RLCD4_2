//
// 5x7 ASCII 字体点阵
// 可打印字符 0x20~0x7F，每字符 5 列，每列 1 字节（bit0=顶行）
//

#pragma once

#include <stdint.h>

// 获取 ASCII 字符的 5x7 点阵，返回指向 5 字节数组的指针（列主序，bit0=顶行）
// 参数 c 应在 0x20~0x7E 范围内，否则返回 nullptr
const uint8_t *Font5x7_GetGlyph(unsigned char c);

// Font 适配器：将列主序数据转换为 1bpp 行主序，供 drawText/drawBitmap 使用
// 返回指向内部静态缓冲区的指针（7 字节，stride=1）
// 支持码点范围：0x20~0x7E（可打印 ASCII），其余返回 nullptr
#include "../Font.h"
const uint8_t *Font5x7_GetGlyphFn(uint32_t codepoint,
                                   int &w, int &h, int &strideBytes,
                                   int &advanceX, const void *data);

extern const Font kFont_ASCII5x7;
