//
// 字体: AlibabaPuHuiTi-3-75-SemiBold
// 尺寸: 72x96
//

#pragma once

#include <cstdint>
#include <cstring>

// 获取字符点阵数据
const uint8_t* getFont96_AlibabaPuHuiTi_3_75_SemiBold_Glyph(char ch);

// 获取字体尺寸
int getFont96_AlibabaPuHuiTi_3_75_SemiBold_Width();
int getFont96_AlibabaPuHuiTi_3_75_SemiBold_Height();
int getFont96_AlibabaPuHuiTi_3_75_SemiBold_Stride();

// Font 适配器
#include "../Font.h"
const uint8_t *Font96_AlibabaPuHuiTi_GetGlyphFn(uint32_t codepoint,
                                                  int &w, int &h, int &strideBytes,
                                                  int &advanceX, const void *data);

extern const Font kFont_Alibaba72x96;