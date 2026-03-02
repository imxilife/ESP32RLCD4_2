//
// 字模效果实现：膨胀算法实现加粗与描边
//

#include "GlyphEffect.h"

namespace {

/// 读取 1bpp 位图 (i,j) 像素，越界返回 0
inline int getPixel(const uint8_t *buf, int w, int h, int stride, int i, int j) {
    if (i < 0 || i >= w || j < 0 || j >= h) return 0;
    int byteIndex = j * stride + (i >> 3);
    int bitIndex = 7 - (i & 7);
    return (buf[byteIndex] >> bitIndex) & 1;
}

/// 设置 1bpp 位图 (i,j) 像素为 1
inline void setPixel(uint8_t *buf, int w, int h, int stride, int i, int j) {
    if (i < 0 || i >= w || j < 0 || j >= h) return;
    int byteIndex = j * stride + (i >> 3);
    int bitIndex = 7 - (i & 7);
    buf[byteIndex] |= (1U << bitIndex);
}

/// 膨胀：radius=0 仅复制，radius>=1 在 (2r+1)x(2r+1) 邻域内任一点为 1 则输出为 1
void dilate(const uint8_t *src, uint8_t *dst, int w, int h, int stride, int radius) {
    const int bufSize = stride * h;
    for (int i = 0; i < bufSize; ++i) dst[i] = 0;

    if (radius <= 0) {
        for (int i = 0; i < bufSize; ++i) dst[i] = src[i];
        return;
    }

    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            if (!getPixel(src, w, h, stride, i, j)) continue;
            for (int dj = -radius; dj <= radius; ++dj) {
                for (int di = -radius; di <= radius; ++di) {
                    setPixel(dst, w, h, stride, i + di, j + dj);
                }
            }
        }
    }
}

}  // namespace

const uint8_t *GlyphEffect_Apply(const uint8_t *src, uint8_t *dst,
                                 int w, int h, int stride,
                                 const BigDigitEffectParams &params) {
    if (!src || !dst || w <= 0 || h <= 0 || stride <= 0) {
        return src;
    }

    int radius = params.boldLevel + params.outlineWidth;
    if (radius <= 0) {
        return src;
    }

    dilate(src, dst, w, h, stride, radius);
    return dst;
}
