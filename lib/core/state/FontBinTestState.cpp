#include "FontBinTestState.h"

#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font24Spiffs.h>

namespace {
constexpr int kSectionX = 10;
constexpr int kSectionY = 24;
constexpr int kSectionW = 380;
constexpr int kSectionH = 220;
constexpr int kTitleH = 22;
constexpr int kLineGap = 34;
}

FontBinTestState::FontBinTestState(Gui& gui)
    : gui_(gui) {}

void FontBinTestState::onEnter() {
    drawScreen();
}

void FontBinTestState::onExit() {}

void FontBinTestState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void FontBinTestState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        requestTransition(StateId::LAUNCH);
    }
}

void FontBinTestState::drawScreen() {
    gui_.clear();
    drawSectionFrame(kSectionY, "font24.bin SPIFFS");

    const Font* font24 = gFont24Spiffs.font();
    if (font24 == nullptr) {
        drawCenteredLine(kSectionX, kSectionY + kTitleH + 56, kSectionW,
                         "font24.bin missing", &kFont_ascii_IBMPlexSans_Medium_20_20);
        return;
    }

    const int firstLineY = kSectionY + kTitleH + 22;
    drawCenteredLine(kSectionX, firstLineY, kSectionW, "ABC 123 !?", font24);
    drawCenteredLine(kSectionX, firstLineY + kLineGap, kSectionW, "中文测试 OK", font24);
    drawCenteredLine(kSectionX, firstLineY + kLineGap * 2, kSectionW, "啊中座 GB2312", font24);
    drawCenteredLine(kSectionX, firstLineY + kLineGap * 3, kSectionW, "SPIFFS Font24", font24);
}

void FontBinTestState::drawSectionFrame(int y, const char* title) {
    gui_.drawRect(kSectionX, y, kSectionW, kSectionH, ColorBlack);
    gui_.fillRect(kSectionX + 1, y + 1, kSectionW - 2, kTitleH, ColorBlack);

    gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    gui_.drawText(kSectionX + 10, y + 2, title, ColorWhite, ColorBlack);
}

void FontBinTestState::drawCenteredLine(int x, int y, int w, const char* text, const Font* font) {
    if (text == nullptr || font == nullptr) {
        return;
    }

    // This diagnostics state validates runtime font metrics before drawing,
    // matching the project rule that font/text changes use real pixel width.
    int textX = x + (w - gui_.measureTextWidth(text, font)) / 2;
    if (textX < x + 4) {
        textX = x + 4;
    }

    gui_.setFont(font);
    gui_.drawText(textX, y, text, ColorBlack, ColorWhite);
}
