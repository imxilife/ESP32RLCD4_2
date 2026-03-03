//
// 字模效果：加粗、描边
// 用于 72x96 等大号数字点阵，可通过参数调整粗细和描边
//

#pragma once

#include <stdint.h>

/// 大号数字效果参数（可调）
struct BigDigitEffectParams {
    int boldLevel;      ///< 加粗等级 0=正常, 1=轻, 2=中, 3=重（膨胀半径）
    int outlineWidth;   ///< 描边宽度 0=无, 1/2=描边厚度（额外膨胀）
};

/// 默认参数：无效果
inline BigDigitEffectParams BigDigitEffectParamsDefault() {
    return {0, 0};
}

/// 对 1bpp 字模应用加粗/描边，输出到 dst（可与 src 相同则原地处理）
/// w,h,stride 为字模尺寸；dst 需至少 stride*h 字节
/// 返回实际使用的字模指针（若有效果则返回 dst，否则返回 src）
const uint8_t *GlyphEffect_Apply(const uint8_t *src, uint8_t *dst,
                                 int w, int h, int stride,
                                 const BigDigitEffectParams &params);
