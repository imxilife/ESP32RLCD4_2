//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 16x16
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_W      = 16;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_H      = 16;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_Stride = 2;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_Glyph[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0x50,0x04,0x48,0x04,0x40,0x3F,0xFC,0x04,0x40,0x05,0x48,0x1E,0x70,0x04,0x60,0x04,0xE4,0x05,0x24,0x18,0x18,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
