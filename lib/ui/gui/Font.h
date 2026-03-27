//
// Font — 通用字体描述符，参考 u8g2 设计
//
// 字体是携带元数据的结构体：行高、per-glyph advance、fallback 链内置其中。
// 所有字形数据格式统一为 1bpp 行主序（每行 strideBytes 字节，高位在左）。
//

#pragma once

#include <stdint.h>

struct Font {
    /// 行高（多行换行时 Y 轴步进，通常 = 字高 + 行间距）
    int lineHeight;

    /// 字形查找函数
    /// - codepoint : Unicode 码点
    /// - w / h     : 输出字形像素宽高
    /// - strideBytes: 输出每行字节数
    /// - advanceX  : 输出光标步进宽度（可 != w，支持变宽字体）
    /// - data      : 字体私有数据（由 Font.data 透传）
    /// 返回 nullptr 表示该码点无字形 → 走 fallback 链或静默跳过
    const uint8_t *(*getGlyph)(uint32_t codepoint,
                               int &w, int &h, int &strideBytes,
                               int &advanceX,
                               const void *data);

    /// 字体私有数据指针，透传给 getGlyph（可为 nullptr）
    const void *data;

    /// 找不到字形时的回退字体（nullptr = 静默跳过该字符）
    const Font *fallback;
};
