//
// Font: AlibabaPuHuiTi-3-65-m-Medium.otf
// Mode: Chinese FontTest single glyph
// Size: 24x24
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_W      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_H      = 24;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_Glyph[72] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x03,0xED,0x80,0x1F,0xED,0xC0,0x1F,0x0C,0xE0,0x03,0x0C,0x70,0x03,0x0C,0x20,0x3F,0xFF,0xF8,0x3F,0xFF,0xF8,0x03,0x0E,0x20,0x03,0x0E,0x70,0x03,0xE6,0x60,0x0F,0xF6,0xE0,0x3F,0xE7,0xC0,0x3F,0x07,0x80,0x03,0x0F,0x10,0x03,0x3F,0x18,0x03,0xFB,0x98,0x0F,0x71,0xF8,0x1F,0x01,0xF0,0x0E,0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
