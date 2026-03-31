#include "FontTestState.h"

#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_24_Var.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_24_Var.h>

namespace {
constexpr const char* kUpperLine1 = "ABCDEFGHIJKLM";
constexpr const char* kUpperLine2 = "NOPQRSTUVWXYZ";
constexpr const char* kLowerLine1 = "abcdefghijklm";
constexpr const char* kLowerLine2 = "nopqrstuvwxyz";

constexpr int kScreenW = 400;
constexpr int kSectionX = 10;
constexpr int kSectionW = 380;
constexpr int kTitleH = 22;
constexpr int kSectionH = 136;
constexpr int kLineH = 24;
constexpr int kRowGap = 4;
}

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
    gui_.clear();

    drawFontSection(10, "IBM24x24", &kFont_ascii_IBMPlexSans_Medium_24_Var);
    drawFontSection(154, "Alibaba24x24", &kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_24_Var);
}

void FontTestState::drawFontSection(int y, const char* title, const Font* font) {
    gui_.drawRect(kSectionX, y, kSectionW, kSectionH, ColorBlack);
    gui_.fillRect(kSectionX + 1, y + 1, kSectionW - 2, kTitleH, ColorBlack);

    gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    gui_.drawText(kSectionX + 10, y + 2, title, ColorWhite, ColorBlack);

    const int textY = y + kTitleH + 8;

    gui_.setFont(font);
    const int x1 = kSectionX + (kSectionW - gui_.measureTextWidth(kUpperLine1, font)) / 2;
    const int x2 = kSectionX + (kSectionW - gui_.measureTextWidth(kUpperLine2, font)) / 2;
    const int x3 = kSectionX + (kSectionW - gui_.measureTextWidth(kLowerLine1, font)) / 2;
    const int x4 = kSectionX + (kSectionW - gui_.measureTextWidth(kLowerLine2, font)) / 2;
    gui_.drawText(x1, textY, kUpperLine1, ColorBlack, ColorWhite);
    gui_.drawText(x2, textY + kLineH + kRowGap, kUpperLine2, ColorBlack, ColorWhite);
    gui_.drawText(x3, textY + (kLineH + kRowGap) * 2, kLowerLine1, ColorBlack, ColorWhite);
    gui_.drawText(x4, textY + (kLineH + kRowGap) * 3, kLowerLine2, ColorBlack, ColorWhite);
}
