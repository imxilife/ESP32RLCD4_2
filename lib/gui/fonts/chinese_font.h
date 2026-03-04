//
// 内置中文 16x16 点阵提供器（仅包含演示用少量汉字）
// 与 Gui::ChineseGlyphProvider 签名一致，可直接传给 setChineseGlyphProvider
//

#pragma once

#include <stdint.h>

// 返回 1bpp 点阵指针（16x16，stride=2），无此字返回 nullptr
const uint8_t *ChineseFont_GetGlyph(uint32_t codepoint, int &width, int &height, int &strideBytes);

// Font 适配器
#include "../Font.h"
const uint8_t *ChineseFont_GetGlyphFn(uint32_t codepoint,
                                       int &w, int &h, int &strideBytes,
                                       int &advanceX, const void *data);

extern const Font kFont_Chinese16x16;  // 纯中文 16x16
extern const Font kFont_Mixed;         // 中文 16x16 → 回退 ASCII 5x7
