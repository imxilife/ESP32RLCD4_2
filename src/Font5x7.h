//
// 5x7 ASCII 字体点阵
// 可打印字符 0x20~0x7F，每字符 5 列，每列 1 字节（bit0=顶行）
//

#pragma once

#include <stdint.h>

// 获取 ASCII 字符的 5x7 点阵，返回指向 5 字节数组的指针
// 参数 c 应在 0x20~0x7E 范围内，否则返回 nullptr
const uint8_t *Font5x7_GetGlyph(unsigned char c);
