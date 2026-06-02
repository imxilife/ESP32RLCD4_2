//
// Font: AlibabaPuHuiTi-3-35-Thin.otf
// Mode: Chinese FontTest single glyph
// Size: 24x24
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_W      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_H      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_Glyph[72] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x76,0xC0,0x1F,0xC6,0x60,0x19,0x06,0x30,0x01,0x06,0x18,0x01,0x06,0x00,0x03,0x06,0x00,0x3F,0xFF,0xFC,0x01,0x02,0x00,0x01,0x02,0x18,0x01,0x02,0x10,0x01,0xF2,0x20,0x0F,0xC3,0x60,0x3D,0x01,0xC0,0x01,0x03,0x80,0x01,0x07,0x04,0x01,0x1D,0x84,0x01,0x70,0x8C,0x07,0x40,0x48,0x1E,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
