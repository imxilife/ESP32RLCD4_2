//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 18x18
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_W      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_H      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_Glyph[54] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x0F,0x28,0x00,0x1C,0x26,0x00,0x04,0x22,0x00,0x06,0x20,0x00,0x3F,0xFF,0x00,0x04,0x22,0x00,0x04,0xA6,0x00,0x0F,0xA4,0x00,0x3C,0x18,0x00,0x04,0x30,0x00,0x04,0xF1,0x00,0x05,0x89,0x00,0x1C,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
