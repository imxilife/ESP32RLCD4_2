//
// Font: AlibabaPuHuiTi-3-45-Light.otf
// Mode: Chinese FontTest single glyph
// Size: 12x12
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_W      = 12;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_H      = 12;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_Stride = 2;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_Glyph[24] = {0x00,0x00,0x00,0x00,0x0F,0x00,0x1A,0x80,0x1F,0xC0,0x0B,0x40,0x1D,0x80,0x39,0x00,0x0F,0x40,0x18,0xC0,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
