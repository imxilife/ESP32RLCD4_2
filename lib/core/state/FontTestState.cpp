#include "FontTestState.h"

#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/Font_ascii_NotoSans_Medium_16_16_FontTest.h>
#include <ui/gui/fonts/Font_ascii_NotoSans_Medium_18_18_FontTest.h>
#include <ui/gui/fonts/Font_ascii_NotoSans_Medium_20_20_FontTest.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18.h>
#include <ui/gui/fonts/Font_ascii_RobotoMono_Light_16_16_FontTest.h>
#include <ui/gui/fonts/Font_ascii_RobotoMono_Light_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_12_12_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_26_26_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_28_28_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_35_Thin_30_30_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_14_14_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_16_16_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_20_20_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_24_24_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_26_26_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_28_28_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_45_Light_30_30_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_12_12_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_14_14_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_16_16_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_20_20_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_26_26_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_28_28_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_30_30_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_12_12_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_14_14_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_16_16_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_20_20_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_22_22_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_24_24_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_26_26_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_28_28_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_s_Medium_30_30_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_12_12_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_14_14_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_16_16_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_22_22_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_26_26_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_28_28_FontTest.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_30_30_FontTest.h>

namespace {
constexpr bool kShowChineseFontTest = false;
constexpr bool kShowEnglishFontTest = true;

constexpr const char* kGlyphText = "\xE6\x88\x91";
constexpr const char* kEnglishText =
    "Everyone has the right to freedom of thought, conscience and religion;";

constexpr int kScreenW = 400;
constexpr int kLeftX = 20;
constexpr int kRightMargin = 20;
constexpr int kGlyphGap = 12;
constexpr int kLabelToGlyphGap = 4;
constexpr int kMaxGlyphH = 30;
constexpr int kChineseRowGap = 6;
constexpr int kSectionGap = 8;
constexpr int kEnglishLabelGap = 8;
constexpr int kEnglishRowGap = 6;
constexpr int kWrapBufferSize = 180;

const Font* const kLabelFont = &kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18;
constexpr const char* kChineseSizeLabels[] = {
    "12", "14", "16", "18", "20", "22", "24", "26", "28", "30",
};

const Font* const kLightFonts[] = {
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_12_12_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_14_14_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_16_16_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_18_18_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_20_20_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_22_22_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_24_24_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_26_26_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_28_28_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_45_Light_30_30_FontTest,
};

const Font* const kThinFonts[] = {
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_12_12_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_14_14_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_16_16_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_18_18_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_20_20_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_22_22_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_24_24_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_26_26_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_28_28_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_35_Thin_30_30_FontTest,
};

const Font* const kMediumSFonts[] = {
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_12_12_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_14_14_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_16_16_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_18_18_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_20_20_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_22_22_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_24_24_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_26_26_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_28_28_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_s_Medium_30_30_FontTest,
};

const Font* const kMediumMFonts[] = {
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_12_12_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_14_14_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_16_16_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_18_18_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_20_20_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_22_22_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_24_24_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_26_26_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_28_28_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_65_m_Medium_30_30_FontTest,
};

const Font* const kSemiBold75Fonts[] = {
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_12_12_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_14_14_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_16_16_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_22_22_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_24_24_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_26_26_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_28_28_FontTest,
    &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_30_30_FontTest,
};

constexpr int kChineseFontCount = sizeof(kLightFonts) / sizeof(kLightFonts[0]);

int fontLineHeight(const Font* font) {
    return (font != nullptr && font->lineHeight > 0) ? font->lineHeight : 0;
}

int chineseRowHeight() {
    return fontLineHeight(kLabelFont) + kLabelToGlyphGap + kMaxGlyphH;
}

int chineseRowWidth(const Font* const* fonts) {
    if (fonts == nullptr) return 0;

    int width = 0;
    for (int i = 0; i < kChineseFontCount; ++i) {
        width += fontLineHeight(fonts[i]);
    }
    width += (kChineseFontCount - 1) * kGlyphGap;
    return width;
}

int appendSegment(char* dst, int dstSize, int dstLen, const char* src, int srcLen) {
    if (dst == nullptr || src == nullptr || dstSize <= 0 || dstLen < 0) return dstLen;

    int outLen = dstLen;
    while (srcLen > 0 && outLen < dstSize - 1) {
        dst[outLen++] = *src++;
        --srcLen;
    }
    dst[outLen] = '\0';
    return outLen;
}
}  // namespace

FontTestState::FontTestState(Gui& gui)
    : gui_(gui) {}

void FontTestState::onEnter() {
    drawScreen();
}

void FontTestState::onExit() {}

void FontTestState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void FontTestState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        requestTransition(StateId::LAUNCH);
    }
}

void FontTestState::drawScreen() {
    gui_.clear(ColorWhite);

    int nextY = 6;
    if (kShowChineseFontTest) {
        nextY = drawChineseFontTest(nextY) + kSectionGap;
    }
    if (kShowEnglishFontTest) {
        drawEnglishFontTest(nextY);
    }
}

int FontTestState::drawChineseFontTest(int topY) {
    // Keep each font family in a separate row so size changes stay comparable.
    int y = topY;
    drawChineseFontRow(y, kLightFonts);
    y += chineseRowHeight() + kChineseRowGap;
    drawChineseFontRow(y, kThinFonts);
    y += chineseRowHeight() + kChineseRowGap;
    drawChineseFontRow(y, kMediumSFonts);
    y += chineseRowHeight() + kChineseRowGap;
    drawChineseFontRow(y, kMediumMFonts);
    y += chineseRowHeight() + kChineseRowGap;
    drawChineseFontRow(y, kSemiBold75Fonts);
    return y + chineseRowHeight();
}

void FontTestState::drawChineseFontRow(int topY, const Font* const* fonts) {
    if (fonts == nullptr) return;

    int x = (kScreenW - chineseRowWidth(fonts)) / 2;
    for (int i = 0; i < kChineseFontCount; ++i) {
        const Font* font = fonts[i];
        if (font == nullptr) continue;
        const int h = fontLineHeight(font);

        gui_.setFont(kLabelFont);
        const int labelW = gui_.measureTextWidth(kChineseSizeLabels[i], kLabelFont);
        gui_.drawText(x + (h - labelW) / 2, topY,
                      kChineseSizeLabels[i], ColorBlack, ColorWhite);

        gui_.setFont(font);
        gui_.drawText(x,
                      topY + fontLineHeight(kLabelFont) + kLabelToGlyphGap + kMaxGlyphH - h,
                      kGlyphText,
                      ColorBlack,
                      ColorWhite);
        x += h + kGlyphGap;
    }
}

void FontTestState::drawEnglishFontTest(int topY) {
    // English rows use true measured width and an 8px label/content gap (two 4px units).
    int y = topY;
    y = drawEnglishSample(y, "16", &kFont_ascii_RobotoMono_Light_16_16_FontTest);
    y += kEnglishRowGap;
    y = drawEnglishSample(y, "18", &kFont_ascii_RobotoMono_Light_18_18_FontTest);
    y += kEnglishRowGap;
    y = drawEnglishSample(y, "16", &kFont_ascii_NotoSans_Medium_16_16_FontTest);
    y += kEnglishRowGap;
    y = drawEnglishSample(y, "18", &kFont_ascii_NotoSans_Medium_18_18_FontTest);
    y += kEnglishRowGap;
    drawEnglishSample(y, "20", &kFont_ascii_NotoSans_Medium_20_20_FontTest);
}

int FontTestState::drawEnglishSample(int topY, const char* sizeLabel, const Font* font) {
    if (font == nullptr || sizeLabel == nullptr) return topY;

    gui_.setFont(font);
    const int labelW = gui_.measureTextWidth(sizeLabel, font);
    const int textX = kLeftX + labelW + kEnglishLabelGap;
    const int maxTextW = kScreenW - kRightMargin - textX;

    gui_.drawText(kLeftX, topY, sizeLabel, ColorBlack, ColorWhite);
    return drawWrappedAsciiText(textX, topY, maxTextW, kEnglishText, font);
}

int FontTestState::drawWrappedAsciiText(int x, int y, int maxWidth,
                                        const char* text, const Font* font) {
    if (text == nullptr || font == nullptr || maxWidth <= 0) return y;

    // Word wrapping is local to this test page and avoids heap use on the embedded target.
    char line[kWrapBufferSize] = {};
    char candidate[kWrapBufferSize] = {};
    int lineLen = 0;
    int cursorY = y;
    const int lineHeight = fontLineHeight(font);

    const char* p = text;
    while (*p != '\0') {
        while (*p == ' ') ++p;
        if (*p == '\0') break;

        const char* wordStart = p;
        int wordLen = 0;
        while (p[wordLen] != '\0' && p[wordLen] != ' ') {
            ++wordLen;
        }

        int candidateLen = 0;
        if (lineLen > 0) {
            candidateLen = appendSegment(candidate, kWrapBufferSize, candidateLen, line, lineLen);
            candidateLen = appendSegment(candidate, kWrapBufferSize, candidateLen, " ", 1);
        }
        candidateLen = appendSegment(candidate, kWrapBufferSize, candidateLen, wordStart, wordLen);

        if (gui_.measureTextWidth(candidate, font) <= maxWidth || lineLen == 0) {
            lineLen = appendSegment(line, kWrapBufferSize, 0, candidate, candidateLen);
        } else {
            gui_.drawText(x, cursorY, line, ColorBlack, ColorWhite);
            cursorY += lineHeight;
            lineLen = appendSegment(line, kWrapBufferSize, 0, wordStart, wordLen);
        }

        p += wordLen;
    }

    if (lineLen > 0) {
        gui_.drawText(x, cursorY, line, ColorBlack, ColorWhite);
        cursorY += lineHeight;
    }

    return cursorY;
}
