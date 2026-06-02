//
// Font: AlibabaPuHuiTi-3-75-SemiBold.otf
// Mode: Chinese FontTest single glyph
// Size: 20x20
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_W      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_H      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_Glyph[60] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xB0,0x00,0x1F,0xF6,0x00,0x3F,0xB3,0x00,0x06,0x31,0x80,0x07,0x39,0x00,0x3F,0xFF,0xC0,0x3F,0xFF,0xC0,0x06,0x19,0x80,0x07,0xDB,0x80,0x3F,0xDF,0x00,0x3F,0x1E,0x00,0x06,0x3C,0x00,0x06,0x7C,0xC0,0x07,0xEC,0xC0,0x1E,0xC7,0x80,0x0E,0x07,0x80,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
