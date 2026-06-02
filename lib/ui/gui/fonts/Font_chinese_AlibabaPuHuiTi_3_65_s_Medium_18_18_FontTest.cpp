//
// Font: AlibabaPuHuiTi-3-65-s-Medium.otf
// Mode: Chinese FontTest single glyph
// Size: 18x18
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_W      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_H      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_Glyph[54] = {0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x68,0x00,0x1F,0xEC,0x00,0x1E,0x6E,0x00,0x06,0x66,0x00,0x3F,0xFF,0x00,0x3F,0xFF,0x00,0x06,0x26,0x00,0x07,0xB6,0x00,0x3F,0xBC,0x00,0x3E,0x38,0x00,0x06,0x73,0x00,0x07,0xFB,0x00,0x1F,0x9F,0x00,0x1C,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
