//
// 字体: AlibabaPuHuiTi-3-75-SemiBold  模式: 中文  尺寸: 20x20
// 字符: 分秒
//

#include "Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec.h"

namespace {

static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_W      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_H      = 20;
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Stride = 3;

struct GlyphEntry {
    uint32_t codepoint;
    uint8_t  data[60];
};

static const GlyphEntry kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Glyphs[] = {
    {0x5206, {0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x98,0x00,0x01,0x98,0x00,0x03,0x9C,0x00,0x07,0x0C,0x00,0x0E,0x0E,0x00,0x1C,0x07,0x00,0x3F,0xFF,0xC0,0x3F,0xFE,0xC0,0x07,0xAE,0x00,0x03,0x86,0x00,0x03,0x8E,0x00,0x03,0x0E,0x00,0x07,0x0C,0x00,0x0E,0x3C,0x00,0x3C,0x7C,0x00,0x18,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, // chr(20998)
    {0x79D2, {0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x8C,0x00,0x1F,0x8C,0x00,0x3F,0x3F,0x00,0x06,0x3F,0x00,0x0F,0x6D,0x80,0x3F,0xED,0x80,0x1F,0xED,0xC0,0x0E,0xCC,0xC0,0x0F,0xCD,0x80,0x1F,0x8F,0x00,0x3F,0x87,0x00,0x36,0x0E,0x00,0x06,0x3C,0x00,0x06,0xF8,0x00,0x07,0xE0,0x00,0x05,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, // chr(31186)
};
static const int kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Count =
    static_cast<int>(sizeof(kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Glyphs) / sizeof(kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Glyphs[0]));

static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,
                               int &stride, int &advX, const void*) {
    for (int i = 0; i < kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Count; ++i) {
        if (kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Glyphs[i].codepoint == cp) {
            w = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_W; h = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_H;
            stride = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Stride; advX = kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_W;
            return kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_Glyphs[i].data;
        }
    }
    return nullptr;
}

} // namespace

const Font kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec = {
    /* lineHeight */ kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec_H,
    /* getGlyph   */ getGlyph,
    /* data       */ nullptr,
    /* fallback   */ nullptr,
};