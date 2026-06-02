//
// Font: AlibabaPuHuiTi-3-75-SemiBold.otf
// Mode: Chinese FontTest single glyph
// Size: 24x24
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_W      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_H      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_Glyph[72] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x00,0x07,0xEE,0xC0,0x1F,0xEE,0xE0,0x1F,0x8E,0x70,0x03,0x8E,0x30,0x03,0x8E,0x10,0x3F,0xFF,0xFC,0x3F,0xFF,0xFC,0x03,0x86,0x30,0x03,0x86,0x38,0x03,0xF7,0x70,0x1F,0xF7,0xE0,0x3F,0xF7,0xE0,0x3F,0x87,0xC0,0x03,0x8F,0x88,0x03,0x9F,0x9C,0x03,0xFF,0x9C,0x0F,0xF1,0xF8,0x0F,0x20,0xF8,0x0E,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
