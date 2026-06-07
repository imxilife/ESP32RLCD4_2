#include "ui_pomodoro.h"

#include <device/display/display_bsp.h>
#include <math.h>
#include <cstdio>
#include <ui/gui/fonts/FontManager.h>

namespace {

constexpr int kScreenW = 400;
constexpr int kScreenH = 300;
constexpr int kSetupTitleY = 16;
constexpr int kSetupTimeY = 70;
constexpr int kSetupLabelGap = 12;
constexpr int kSetupBtnTextY = 244;
constexpr int kSetupBtnBorderY = 236;
constexpr int kSetupBtnH = 36;
constexpr int kSetupBtnPad = 8;
constexpr int kNumBgPad = 8;
constexpr int kColonW = 18;

constexpr int kRingCX = 200;
constexpr int kRingCY = 150;
constexpr int kRingInnerR = 117;
constexpr int kRingInner5R = 109;
constexpr int kRingOuterR = 125;
constexpr int kRingTicks = 60;

const Font* enFont() {
    return FontManager::instance().font(FontId::EnMain);
}

const Font* zhFont() {
    return FontManager::instance().font(FontId::ZhSub);
}

const Font* digitsFont() {
    return FontManager::instance().font(FontId::Digits60);
}

void drawCenteredText(Gui& gui, int centerX, int y, const char* text, const Font* font,
                      uint8_t fg = ColorBlack, uint8_t bg = ColorWhite) {
    if (text == nullptr || font == nullptr) return;
    const int textW = gui.measureTextWidth(text, font);
    gui.setFont(font);
    gui.drawText(centerX - textW / 2, y, text, fg, bg);
}

void drawColon(Gui& gui, int centerX, int topY, int height, uint8_t color) {
    gui.fillCircle(centerX, topY + height * 34 / 100, 5, color);
    gui.fillCircle(centerX, topY + height * 66 / 100, 5, color);
}

void drawLargePair(Gui& gui, int topY, const char* left, const char* right,
                   int* leftXOut = nullptr, int* leftWOut = nullptr,
                   int* rightXOut = nullptr, int* rightWOut = nullptr) {
    const Font* font = digitsFont();
    if (font == nullptr) return;

    const int leftW = gui.measureTextWidth(left, font);
    const int rightW = gui.measureTextWidth(right, font);
    const int totalW = leftW + kColonW + rightW;
    int x = (kScreenW - totalW) / 2;
    if (x < 0) x = 0;

    gui.setFont(font);
    gui.drawText(x, topY, left, ColorBlack, ColorWhite);
    drawColon(gui, x + leftW + kColonW / 2, topY, font->lineHeight, ColorBlack);
    gui.drawText(x + leftW + kColonW, topY, right, ColorBlack, ColorWhite);

    if (leftXOut != nullptr) *leftXOut = x;
    if (leftWOut != nullptr) *leftWOut = leftW;
    if (rightXOut != nullptr) *rightXOut = x + leftW + kColonW;
    if (rightWOut != nullptr) *rightWOut = rightW;
}

void drawProgressRing(Gui& gui, uint32_t remSec, uint32_t totalSec) {
    int filled = (totalSec > 0)
                 ? static_cast<int>((uint64_t)remSec * kRingTicks / totalSec)
                 : 0;
    if (filled > kRingTicks) filled = kRingTicks;

    for (int i = 0; i < kRingTicks; i++) {
        float angle = -(float)M_PI_2 + (2.0f * (float)M_PI * i) / kRingTicks;
        float ca = cosf(angle);
        float sa = sinf(angle);

        bool isMark = (i % 5 == 0);
        int innerR = isMark ? kRingInner5R : kRingInnerR;

        int x1 = kRingCX + static_cast<int>(innerR * ca);
        int y1 = kRingCY + static_cast<int>(innerR * sa);
        int x2 = kRingCX + static_cast<int>(kRingOuterR * ca);
        int y2 = kRingCY + static_cast<int>(kRingOuterR * sa);

        uint8_t color = (i < filled) ? ColorBlack : ColorWhite;
        int px = (fabsf(sa) >= fabsf(ca)) ? 1 : 0;
        int py = (fabsf(sa) >= fabsf(ca)) ? 0 : 1;
        gui.drawLine(x1, y1, x2, y2, color);
        gui.drawLine(x1 + px, y1 + py, x2 + px, y2 + py, color);
    }
}

void drawTimeLarge(Gui& gui, uint32_t remSec) {
    const Font* digitFont = digitsFont();
    if (digitFont == nullptr) return;

    char sBuf[3];
    snprintf(sBuf, sizeof(sBuf), "%02d", static_cast<int>(remSec));
    const int textW = gui.measureTextWidth(sBuf, digitFont);
    const int x = (kScreenW - textW) / 2;
    const int y = 82;
    gui.fillRect(0, y - 4, kScreenW, digitFont->lineHeight + 36, ColorWhite);
    gui.setFont(digitFont);
    gui.drawText(x, y, sBuf, ColorBlack, ColorWhite);
    drawCenteredText(gui, kScreenW / 2, y + digitFont->lineHeight + 8, "sec", enFont());
}

void drawTimeSmall(Gui& gui, const char* fmtStr) {
    drawCenteredText(gui, kScreenW / 2, 140, fmtStr, enFont());
}

void drawFocusedDigits(Gui& gui, int x, int y, int w, const char* text, bool focused) {
    const Font* font = digitsFont();
    if (font == nullptr) return;
    const int bgX = x - kNumBgPad;
    const int bgY = y - kNumBgPad;
    const int bgW = w + kNumBgPad * 2;
    const int bgH = font->lineHeight + kNumBgPad * 2;
    if (focused) {
        gui.fillRoundRect(bgX, bgY, bgW, bgH, 8, ColorBlack);
        gui.setFont(font);
        gui.drawText(x, y, text, ColorWhite, ColorTransparent);
    } else {
        gui.fillRect(bgX, bgY, bgW, bgH, ColorWhite);
        gui.setFont(font);
        gui.drawText(x, y, text, ColorBlack, ColorWhite);
    }
}

void drawSetup(Gui& gui, uint8_t setH, uint8_t setM, uint8_t focus) {
    gui.clear();
    drawCenteredText(gui, kScreenW / 2, kSetupTitleY, "POMODORO", enFont());

    char hBuf[3];
    char mBuf[3];
    snprintf(hBuf, sizeof(hBuf), "%02d", setH);
    snprintf(mBuf, sizeof(mBuf), "%02d", setM);

    int hX = 0;
    int hW = 0;
    int mX = 0;
    int mW = 0;
    drawLargePair(gui, kSetupTimeY, hBuf, mBuf, &hX, &hW, &mX, &mW);
    drawFocusedDigits(gui, hX, kSetupTimeY, hW, hBuf, focus == 0);
    drawFocusedDigits(gui, mX, kSetupTimeY, mW, mBuf, focus == 1);
    const Font* digitFont = digitsFont();
    const int digitLineH = digitFont != nullptr ? digitFont->lineHeight : 60;
    drawColon(gui, hX + hW + kColonW / 2, kSetupTimeY, digitLineH, ColorBlack);

    const int setupLabelY = kSetupTimeY + digitLineH + kSetupLabelGap;
    drawCenteredText(gui, hX + hW / 2, setupLabelY, "\xe5\x88\x86", zhFont());
    drawCenteredText(gui, mX + mW / 2, setupLabelY, "\xe7\xa7\x92", zhFont());

    const Font* font = enFont();
    const int exitW = gui.measureTextWidth("EXIT", font);
    const int startW = gui.measureTextWidth("START", font);

    const int exitX = 100 - exitW / 2;
    bool exitFocused = (focus == 2);
    int exitRx = exitX - kSetupBtnPad;
    int exitRw = exitW + 2 * kSetupBtnPad;
    if (exitFocused) {
        gui.fillRoundRect(exitRx, kSetupBtnBorderY, exitRw, kSetupBtnH, 4, ColorBlack);
    } else {
        gui.fillRoundRect(exitRx, kSetupBtnBorderY, exitRw, kSetupBtnH, 4, ColorWhite);
        gui.drawRoundRect(exitRx, kSetupBtnBorderY, exitRw, kSetupBtnH, 4, ColorBlack);
    }
    gui.setFont(font);
    gui.drawText(exitX, kSetupBtnTextY, "EXIT",
                 exitFocused ? ColorWhite : ColorBlack,
                 exitFocused ? ColorBlack : ColorWhite);

    const int startX = 300 - startW / 2;
    bool startFocused = (focus == 3);
    int startRx = startX - kSetupBtnPad;
    int startRw = startW + 2 * kSetupBtnPad;
    if (startFocused) {
        gui.fillRoundRect(startRx, kSetupBtnBorderY, startRw, kSetupBtnH, 4, ColorBlack);
    } else {
        gui.fillRoundRect(startRx, kSetupBtnBorderY, startRw, kSetupBtnH, 4, ColorWhite);
        gui.drawRoundRect(startRx, kSetupBtnBorderY, startRw, kSetupBtnH, 4, ColorBlack);
    }
    gui.drawText(startX, kSetupBtnTextY, "START",
                 startFocused ? ColorWhite : ColorBlack,
                 startFocused ? ColorBlack : ColorWhite);
}

void drawCountdown(Gui& gui, uint32_t remSec, uint32_t totalSec) {
    gui.fillRect(0, 0, kScreenW, kScreenH, ColorWhite);
    drawProgressRing(gui, remSec, totalSec);
    if (remSec <= 60) {
        drawTimeLarge(gui, remSec);
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 static_cast<int>(remSec / 60),
                 static_cast<int>(remSec % 60));
        drawTimeSmall(gui, buf);
    }
}

void drawFinished(Gui& gui, bool blinkOn) {
    uint8_t bg = blinkOn ? ColorWhite : ColorBlack;
    uint8_t fg = blinkOn ? ColorBlack : ColorWhite;
    gui.fillRect(0, 0, kScreenW, kScreenH, bg);
    drawCenteredText(gui, kScreenW / 2, (kScreenH - 18) / 2,
                     "DONE! HOLD KEY1", enFont(), fg, bg);
}

}  // namespace

void handlePomodoroUpdate(Gui& gui, const AppMessage& msg) {
    const PomodoroMsg& p = msg.pomodoro;
    switch (p.event) {
    case PomodoroEvent::SETUP:
        drawSetup(gui, p.setup.hours, p.setup.minutes, p.setup.focus);
        break;
    case PomodoroEvent::TICK:
        drawCountdown(gui, p.tick.remSec, p.tick.totalSec);
        break;
    case PomodoroEvent::FINISHED:
        drawFinished(gui, p.finished.blinkOn);
        break;
    case PomodoroEvent::EXIT:
        break;
    }
}
