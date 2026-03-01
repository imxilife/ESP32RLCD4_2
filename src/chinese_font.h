//
// 内置中文 16x16 点阵提供器（仅包含演示用少量汉字）
// 与 Gui::ChineseGlyphProvider 签名一致，可直接传给 setChineseGlyphProvider
//

#pragma once

#include <stdint.h>

// 返回 1bpp 点阵指针（16x16，stride=2），无此字返回 nullptr
const uint8_t *ChineseFont_GetGlyph(uint32_t codepoint, int &width, int &height, int &strideBytes);
