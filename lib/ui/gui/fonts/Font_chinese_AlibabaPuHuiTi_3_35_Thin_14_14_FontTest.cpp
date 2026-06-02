//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 14x14
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_W      = 14;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_H      = 14;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_Stride = 2;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_Glyph[28] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1D,0x60,0x08,0x00,0x3F,0xF0,0x08,0x80,0x0E,0x20,0x38,0xC0,0x08,0x80,0x0B,0x50,0x18,0x60,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
