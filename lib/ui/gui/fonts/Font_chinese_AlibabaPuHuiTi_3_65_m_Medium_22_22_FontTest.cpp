//
// Font: AlibabaPuHuiTi-3-65-m-Medium.otf
// Mode: Chinese FontTest single glyph
// Size: 22x22
// Glyph: U+6211
// Generated for FontTestState only.
//

#include "Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest.h"

namespace {
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_W      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_H      = 22;
static const int kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_Stride = 3;
static const uint8_t kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_Glyph[66] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x07,0xFF,0x80,0x1F,0xDD,0xC0,0x1F,0x1C,0xE0,0x03,0x0C,0x40,0x3F,0xFF,0xF0,0x3F,0xFF,0xF0,0x03,0x1C,0x40,0x03,0x0C,0x60,0x03,0xEC,0xE0,0x1F,0xED,0xC0,0x3F,0x8F,0x80,0x3B,0x0F,0x00,0x03,0x1E,0x30,0x03,0x7F,0x30,0x03,0xF3,0x70,0x1F,0x43,0xE0,0x0E,0x01,0xC0,0x00,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    if (cp != 0x6211) return nullptr;
    w = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_W; h = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_H;
    stride = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_W;
    return kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_Glyph;
}
} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};
