//
// Font: AlibabaPuHuiTi-3-45-Light.otf
// Mode: Chinese FontTest single glyph
// Size: 18x18
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_W      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_H      = 18;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_Glyph[54] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x1F,0x58,0x00,0x3C,0x4C,0x00,0x04,0x44,0x00,0x3F,0xFE,0x00,0x3F,0xFE,0x00,0x04,0x64,0x00,0x05,0xEC,0x00,0x3F,0x38,0x00,0x34,0x30,0x00,0x04,0x62,0x00,0x05,0xF2,0x00,0x0D,0x1E,0x00,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
