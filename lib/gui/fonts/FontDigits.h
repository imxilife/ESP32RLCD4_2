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
