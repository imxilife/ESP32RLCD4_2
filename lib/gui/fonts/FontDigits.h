//
// 大/小号数字字体点阵（仅 0-9、':'、'.'）
// 大号：72x96，Arial Black，笔画端部圆弧，冒号为实心圆点
// 小号：24x32，DIN Condensed Bold，瘦长，小数点底部对齐实心圆
//

#pragma once

#include <stdint.h>

// 返回大号 72x96 字体点阵（1bpp，行主序，strideBytes = (width+7)/8 = 9）
const uint8_t *FontBig72x96_GetGlyph(char ch, int &width, int &height, int &strideBytes);

// 返回小号 24x32 字体点阵（1bpp，行主序，strideBytes = (width+7)/8 = 3）
const uint8_t *FontSmall24x32_GetGlyph(char ch, int &width, int &height, int &strideBytes);

// Font 适配器
#include "../Font.h"
const uint8_t *FontBig72x96_GetGlyphFn(uint32_t codepoint,
                                        int &w, int &h, int &strideBytes,
                                        int &advanceX, const void *data);
const uint8_t *FontSmall24x32_GetGlyphFn(uint32_t codepoint,
                                          int &w, int &h, int &strideBytes,
                                          int &advanceX, const void *data);

extern const Font kFont_BigDigit;    // 72x96 大号数字
extern const Font kFont_SmallDigit;  // 24x32 小号数字
