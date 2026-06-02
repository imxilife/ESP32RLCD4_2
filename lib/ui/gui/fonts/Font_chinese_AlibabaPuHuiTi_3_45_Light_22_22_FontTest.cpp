//
// Font: AlibabaPuHuiTi-3-45-Light.otf
// Mode: Chinese FontTest single glyph
// Size: 22x22
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_W      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_H      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_Glyph[66] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x01,0xE9,0x00,0x1F,0xCD,0x80,0x13,0x0C,0xC0,0x03,0x0C,0x60,0x03,0x0C,0x00,0x3F,0xFF,0xF0,0x03,0x0C,0x00,0x03,0x0C,0x60,0x03,0x2C,0x40,0x03,0xE4,0xC0,0x3F,0x87,0x80,0x33,0x07,0x00,0x03,0x0E,0x10,0x03,0x3E,0x30,0x03,0x73,0x30,0x0F,0x41,0xE0,0x1E,0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
