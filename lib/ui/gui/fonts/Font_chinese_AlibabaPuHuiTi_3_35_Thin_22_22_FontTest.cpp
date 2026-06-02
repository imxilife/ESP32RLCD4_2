//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 22x22
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_W      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_H      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_Glyph[66] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x01,0xDA,0x00,0x1F,0x19,0x00,0x02,0x18,0x80,0x02,0x08,0x40,0x02,0x08,0x00,0x3F,0xFF,0xE0,0x02,0x08,0x00,0x02,0x08,0x40,0x02,0x08,0x80,0x03,0xE9,0x80,0x3F,0x0F,0x00,0x22,0x06,0x00,0x02,0x0C,0x20,0x02,0x36,0x20,0x02,0xE2,0x60,0x0E,0x83,0x40,0x1C,0x01,0xC0,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
