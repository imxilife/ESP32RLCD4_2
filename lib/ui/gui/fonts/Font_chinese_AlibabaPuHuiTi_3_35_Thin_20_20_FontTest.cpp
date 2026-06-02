//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 20x20
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_W      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_H      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_Glyph[60] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,0xD6,0x00,0x1E,0x13,0x00,0x02,0x11,0x80,0x02,0x10,0x00,0x3F,0xFF,0xC0,0x02,0x18,0x00,0x02,0x18,0x80,0x02,0x49,0x80,0x07,0xCB,0x00,0x3E,0x0E,0x00,0x02,0x0C,0x00,0x02,0x3C,0x40,0x02,0x64,0x40,0x06,0x82,0xC0,0x1C,0x03,0x80,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
