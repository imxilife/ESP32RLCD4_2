#include "LaunchState.h"

#include <Arduino.h>
#include <core/app_tasks/app_tasks.h>
#include <core/state_manager/StateManager.h>
#include <device/display/display_bsp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <media/mp3_controller/Mp3Controller.h>
#include <cstdio>
#include <cstring>
#include <ui/assets/Mp3ControlIcons.h>
#include <ui/gui/Font.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSansCondensed_SemiBold_40_64.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_24_Var.h>
#include <ui/gui/fonts/Font_ascii_Oswald_Light_28_40.h>

namespace {
constexpr const char* kWeekLabels[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

const Font* kTimeDigitFont = &kFont_ascii_IBMPlexSansCondensed_SemiBold_40_64;
const Font* kEnglishUiFont = &kFont_ascii_IBMPlexSans_Medium_20_20;
const Font* kCardEnglishFont = &kFont_ascii_IBMPlexSans_Medium_24_Var;
const Font* kMusicAsciiTitleFont = &kFont_ascii_IBMPlexSans_Medium_24_Var;
const Font* kMusicChineseTitleFont = &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20;

bool isAsciiText(const String& text) {
    for (size_t i = 0; i < text.length(); ++i) {
        if (static_cast<uint8_t>(text[i]) > 0x7F) return false;
    }
    return true;
}

const Font* musicTitleFont(const String& text) {
    return isAsciiText(text) ? kMusicAsciiTitleFont : kMusicChineseTitleFont;
}

String fitTextToWidth(Gui& gui, const String& text, const Font* font, int maxWidth) {
    if (text.isEmpty() || font == nullptr) return text;
    if (gui.measureTextWidth(text.c_str(), font) <= maxWidth) return text;

    String result = text;
    while (result.length() > 1) {
        result.remove(result.length() - 1);
        String candidate = result + "...";
        if (gui.measureTextWidth(candidate.c_str(), font) <= maxWidth) return candidate;
    }
    return "...";
}

const char* musicStateLabel(Mp3Controller::State state) {
    switch (state) {
    case Mp3Controller::State::UNINITIALIZED: return "INIT";
    case Mp3Controller::State::IDLE: return "READY";
    case Mp3Controller::State::PLAYING: return "PLAYING";
    case Mp3Controller::State::PAUSED: return "PAUSED";
    case Mp3Controller::State::STOPPED: return "STOPPED";
    case Mp3Controller::State::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

bool getDigitGlyph(char ch, const uint8_t*& glyph, int& w, int& h, int& stride) {
    if (kTimeDigitFont == nullptr || kTimeDigitFont->getGlyph == nullptr) return false;

    int advanceX = 0;
    glyph = kTimeDigitFont->getGlyph(static_cast<uint8_t>(ch), w, h, stride, advanceX, kTimeDigitFont->data);
    return glyph != nullptr && w > 0 && h > 0 && stride > 0;
}
}

LaunchState::LaunchState(Gui& gui)
    : gui_(gui) {}

void LaunchState::onEnter() {
    static bool rtcStarted = false;
    if (!rtcStarted) {
        rtcStarted = true;
        xTaskCreate(rtcTask, "rtcTask", 2048, &rtc_, 2, nullptr);
        Serial.println("[Launch] RTC task started");
    }

    focus_ = Focus::TIME;
    timeAnim_.active = false;
    if (hasRtc_) {
        displayedTime_ = currentTime_;
    }
    Mp3Controller::instance().beginIfNeeded();
    drawFrame();
    draw();
}

void LaunchState::onExit() {}

void LaunchState::onMessage(const AppMessage& msg) {
    if (msg.type == MSG_NTP_SYNC) {
        rtc_.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                     msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                     msg.ntpTime.weekday);
        Serial.println("[Launch] NTP time written to RTC");

        currentTime_.year = msg.ntpTime.year;
        currentTime_.month = msg.ntpTime.month;
        currentTime_.day = msg.ntpTime.day;
        currentTime_.hour = msg.ntpTime.hour;
        currentTime_.minute = msg.ntpTime.minute;
        currentTime_.second = msg.ntpTime.second;
        currentTime_.weekday = msg.ntpTime.weekday;
        displayedTime_ = currentTime_;
        hasRtc_ = true;
        timeAnim_.active = false;
        drawTimeCard();
        drawDateCard();
        return;
    }

    if (msg.type != MSG_RTC_UPDATE) return;

    const bool hadRtc = hasRtc_;
    const RTCTime previousTime = currentTime_;
    currentTime_ = msg.rtcTime;
    hasRtc_ = true;

    if (!hadRtc) {
        displayedTime_ = currentTime_;
        timeAnim_.active = false;
        drawTimeCard();
        drawDateCard();
        return;
    }

    if (previousTime.hour != currentTime_.hour || previousTime.minute != currentTime_.minute) {
        startTimeAnimation(previousTime.hour, previousTime.minute, currentTime_.hour, currentTime_.minute);
    }

    if (previousTime.year != currentTime_.year ||
        previousTime.month != currentTime_.month ||
        previousTime.day != currentTime_.day ||
        previousTime.weekday != currentTime_.weekday) {
        drawDateCard();
    }
}

void LaunchState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        Focus previous = focus_;
        focus_ = nextFocus(focus_);
        redrawFocus(previous, focus_);
        return;
    }

    if (event.id != KeyId::KEY2) return;

    switch (focus_) {
    case Focus::TIME:
    case Focus::DATE:
        requestTransition(StateId::MAIN_UI);
        break;
    case Focus::INFO:
        requestTransition(StateId::POMODORO);
        break;
    case Focus::MUSIC:
        Serial.println("[Launch] Enter MusicPlayerState from MP3 card");
        requestTransition(StateId::MUSIC_PLAYER);
        break;
    }
}

void LaunchState::tick() {
    Mp3Controller::instance().tick();
    drawMusicCard();

    if (!timeAnim_.active) return;

    const uint32_t elapsed = millis() - timeAnim_.startMs;
    if (elapsed >= timeAnim_.durationMs) {
        finishTimeAnimation();
        return;
    }

    const uint8_t offset = static_cast<uint8_t>((elapsed * kTimeDigitH) / timeAnim_.durationMs);
    drawTimeDigitsAnimated(kTimeCardX, kTimeDigitsY, offset);
}

void LaunchState::drawFrame() {
    gui_.fillRect(0, 0, kScreenW, kScreenH, ColorBlack);
}

void LaunchState::draw() {
    drawTimeCard();
    drawDateCard();
    drawInfoCard();
    drawMusicCard();
}

void LaunchState::redrawFocus(Focus previous, Focus current) {
    if (current == previous) return;

    // 焦点切换只改动两条内圈描边，避免重绘卡片内容导致闪烁。
    drawFocusByState(previous, false);
    drawFocusByState(current, true);
}

void LaunchState::drawCard(int x, int y, int w, int h) {
    gui_.fillRoundRect(x, y, w, h, kCardRadius, ColorWhite);
    gui_.drawRoundRect(x, y, w, h, kCardRadius, ColorBlack);
}

void LaunchState::drawFocusRings(int x, int y, int w, int h, uint8_t color) {
    gui_.drawRoundRect(x + kFocusInset1, y + kFocusInset1,
                       w - kFocusInset1 * 2, h - kFocusInset1 * 2,
                       kCardRadius - kFocusInset1, color);
    gui_.drawRoundRect(x + kFocusInset2, y + kFocusInset2,
                       w - kFocusInset2 * 2, h - kFocusInset2 * 2,
                       kCardRadius - kFocusInset2, color);
}

void LaunchState::drawFocusByState(Focus focus, bool focused) {
    uint8_t color = focused ? ColorBlack : ColorWhite;
    switch (focus) {
    case Focus::TIME:
        drawFocusRings(kTimeCardX, kTopY, kTimeCardW, kTopH, color);
        break;
    case Focus::DATE:
        drawFocusRings(kDateCardX, kTopY, kDateCardW, kTopH, color);
        break;
    case Focus::INFO:
        drawFocusRings(kInfoCardX, kBottomY, kInfoCardW, kBottomH, color);
        break;
    case Focus::MUSIC:
        drawFocusRings(kMusicCardX, kBottomY, kMusicCardW, kBottomH, color);
        break;
    }
}

void LaunchState::drawCenteredText(int x, int y, int w, const char* text, const Font* font,
                                   uint8_t fg, uint8_t bg) {
    if (text == nullptr || font == nullptr) return;

    int textX = x + (w - gui_.measureTextWidth(text, font)) / 2;
    if (textX < x + 4) textX = x + 4;

    gui_.setFont(font);
    gui_.drawText(textX, y, text, fg, bg);
}

void LaunchState::drawCenteredMeasuredText(int x, int y, int w, const char* text, const Font* font,
                                           uint8_t fg, uint8_t bg) {
    if (text == nullptr || font == nullptr) return;

    int textX = x + (w - gui_.measureTextWidth(text, font)) / 2;
    if (textX < x + 4) textX = x + 4;

    gui_.setFont(font);
    gui_.drawText(textX, y, text, fg, bg);
}

void LaunchState::formatTimeDigits(char out[5], uint8_t hour, uint8_t minute) const {
    snprintf(out, 5, "%02u%02u", hour, minute);
}

int LaunchState::timeDigitsStartX(int x) const {
    const int totalDigitsW = kTimeDigitW * 4;
    const int totalGapW = kTimeDigitGap * 2 + kTimeGroupGap;
    const int totalW = totalDigitsW + totalGapW;
    return x + (kTimeCardW - totalW) / 2;
}

int LaunchState::timeDigitX(int cardX, int index) const {
    int cursorX = timeDigitsStartX(cardX);
    if (index >= 1) cursorX += kTimeDigitW + kTimeDigitGap;
    if (index >= 2) cursorX += kTimeDigitW + kTimeGroupGap;
    if (index >= 3) cursorX += kTimeDigitW + kTimeDigitGap;
    return cursorX;
}

void LaunchState::drawTimeColon(int x, int y) {
    const int startX = timeDigitsStartX(x);
    const int colonCenterX = startX + kTimeDigitW * 2 + kTimeDigitGap + kTimeGroupGap / 2;
    const int colonUpperY = y + kTimeDigitH / 2 - kTimeColonGap;
    const int colonLowerY = y + kTimeDigitH / 2 + kTimeColonGap;
    gui_.fillCircle(colonCenterX, colonUpperY, kTimeColonRadius, ColorBlack);
    gui_.fillCircle(colonCenterX, colonLowerY, kTimeColonRadius, ColorBlack);
}

void LaunchState::drawDigitGlyphClipped(int x, int y,
                                        const uint8_t* glyph, int glyphW, int glyphH, int stride,
                                        int clipX, int clipY, int clipW, int clipH) {
    if (glyph == nullptr || glyphW <= 0 || glyphH <= 0 || stride <= 0) return;

    const int srcStartY = (clipY > y) ? (clipY - y) : 0;
    const int srcEndY = ((clipY + clipH) < (y + glyphH)) ? (clipY + clipH - y) : glyphH;
    const int srcStartX = (clipX > x) ? (clipX - x) : 0;
    const int srcEndX = ((clipX + clipW) < (x + glyphW)) ? (clipX + clipW - x) : glyphW;

    if (srcStartY >= srcEndY || srcStartX >= srcEndX) return;

    for (int row = srcStartY; row < srcEndY; ++row) {
        const uint8_t* rowData = glyph + row * stride;
        for (int col = srcStartX; col < srcEndX; ++col) {
            const uint8_t mask = static_cast<uint8_t>(0x80 >> (col & 7));
            if ((rowData[col >> 3] & mask) == 0) continue;
            gui_.drawPixel(x + col, y + row, ColorBlack);
        }
    }
}

void LaunchState::drawTimeDigitCell(int x, int y, char digit) {
    const uint8_t* glyph = nullptr;
    int glyphW = 0, glyphH = 0, stride = 0;

    gui_.fillRect(x, y, kTimeDigitW, kTimeDigitH, ColorWhite);
    if (!getDigitGlyph(digit, glyph, glyphW, glyphH, stride)) return;
    gui_.drawBitmap(x, y, glyphW, glyphH, glyph, ColorBlack);
}

void LaunchState::drawTimeDigits(int x, int y, uint8_t hour, uint8_t minute) {
    char digits[5];
    formatTimeDigits(digits, hour, minute);

    for (int i = 0; i < 4; ++i) {
        drawTimeDigitCell(timeDigitX(x, i), y, digits[i]);
    }

    drawTimeColon(x, y);
}

void LaunchState::drawTimeDigitsAnimated(int x, int y, uint8_t offset) {
    const int clipY = y;

    for (int i = 0; i < 4; ++i) {
        const int digitX = timeDigitX(x, i);
        gui_.fillRect(digitX, y, kTimeDigitW, kTimeDigitH, ColorWhite);

        if (timeAnim_.fromDigits[i] == timeAnim_.toDigits[i]) {
            drawTimeDigitCell(digitX, y, timeAnim_.toDigits[i]);
            continue;
        }

        const uint8_t* fromGlyph = nullptr;
        const uint8_t* toGlyph = nullptr;
        int fromW = 0, fromH = 0, fromStride = 0;
        int toW = 0, toH = 0, toStride = 0;

        if (getDigitGlyph(timeAnim_.fromDigits[i], fromGlyph, fromW, fromH, fromStride)) {
            drawDigitGlyphClipped(digitX, y - offset,
                                  fromGlyph, fromW, fromH, fromStride,
                                  digitX, clipY, kTimeDigitW, kTimeDigitH);
        }

        if (getDigitGlyph(timeAnim_.toDigits[i], toGlyph, toW, toH, toStride)) {
            drawDigitGlyphClipped(digitX, y + kTimeDigitH - offset,
                                  toGlyph, toW, toH, toStride,
                                  digitX, clipY, kTimeDigitW, kTimeDigitH);
        }
    }

    drawTimeColon(x, y);
}

void LaunchState::drawTimeCard() {
    const uint8_t hour = hasRtc_ ? displayedTime_.hour : 0;
    const uint8_t minute = hasRtc_ ? displayedTime_.minute : 0;

    drawCard(kTimeCardX, kTopY, kTimeCardW, kTopH);
    if (timeAnim_.active) {
        const uint32_t elapsed = millis() - timeAnim_.startMs;
        const uint8_t offset = (elapsed >= timeAnim_.durationMs)
                                 ? kTimeDigitH
                                 : static_cast<uint8_t>((elapsed * kTimeDigitH) / timeAnim_.durationMs);
        drawTimeDigitsAnimated(kTimeCardX, kTimeDigitsY, offset);
    } else {
        drawTimeDigits(kTimeCardX, kTimeDigitsY, hour, minute);
    }
    if (focus_ == Focus::TIME) {
        drawFocusRings(kTimeCardX, kTopY, kTimeCardW, kTopH, ColorBlack);
    }
}

void LaunchState::drawDateCard() {
    char dayBuf[4];
    uint8_t day = hasRtc_ ? currentTime_.day : 0;
    uint8_t weekday = (hasRtc_ && currentTime_.weekday < 7) ? currentTime_.weekday : 0;
    snprintf(dayBuf, sizeof(dayBuf), "%02u", day);

    drawCard(kDateCardX, kTopY, kDateCardW, kTopH);

    gui_.fillRoundRect(kDateCardX + 8, kTopY + 8, kDateCardW - 16, 26, kDateWeekRadius, ColorBlack);
    drawCenteredText(kDateCardX, kTopY + 11, kDateCardW, kWeekLabels[weekday],
                     kEnglishUiFont, ColorWhite, ColorBlack);

    gui_.setFont(&kFont_ascii_Oswald_Light_28_40);
    const int dayTextW = 56;
    const int dayTextX = kDateCardX + (kDateCardW - dayTextW) / 2;
    const int dayTextY = kTopY + 52;
    gui_.drawText(dayTextX, dayTextY, dayBuf, ColorBlack, ColorWhite);

    // 模拟纸质日历数字中部断缝，只覆写数字区域，避免破坏卡片背景。
    gui_.fillRect(dayTextX - 2, dayTextY + kDateSplitOffsetY, dayTextW + 4, kDateSplitGap, ColorWhite);

    if (focus_ == Focus::DATE) {
        drawFocusRings(kDateCardX, kTopY, kDateCardW, kTopH, ColorBlack);
    }
}

void LaunchState::drawInfoCard() {
    drawCard(kInfoCardX, kBottomY, kInfoCardW, kBottomH);
    drawCenteredMeasuredText(kInfoCardX, kBottomY + 34, kInfoCardW, "Pomodoro",
                             kCardEnglishFont, ColorBlack, ColorWhite);
    drawCenteredMeasuredText(kInfoCardX, kBottomY + 74, kInfoCardW, "Timer",
                             kCardEnglishFont, ColorBlack, ColorWhite);
    if (focus_ == Focus::INFO) {
        drawFocusRings(kInfoCardX, kBottomY, kInfoCardW, kBottomH, ColorBlack);
    }
}

void LaunchState::drawMusicCard() {
    drawCard(kMusicCardX, kBottomY, kMusicCardW, kBottomH);

    const Mp3Controller::Snapshot snap = Mp3Controller::instance().snapshot();
    const int innerX = kMusicCardX + 16;
    const int innerW = kMusicCardW - 32;

    String title = "NO MUSIC";
    if (snap.hasPlaylist && !snap.track.title.isEmpty()) {
        const Font* titleFont = musicTitleFont(snap.track.title);
        title = fitTextToWidth(gui_, snap.track.title, titleFont, innerW);
    } else if (!snap.errorText.isEmpty()) {
        title = fitTextToWidth(gui_, snap.errorText, kMusicAsciiTitleFont, innerW);
    }

    gui_.setFont(musicTitleFont(title));
    gui_.drawText(innerX, kBottomY + 18, title.c_str(), ColorBlack, ColorWhite);

    char infoLine[48];
    if (snap.hasPlaylist && snap.playlistCount > 0) {
        const int displayIndex = (snap.currentIndex >= 0) ? (snap.currentIndex + 1) : (snap.selectedIndex + 1);
        snprintf(infoLine, sizeof(infoLine), "%s  %d/%d",
                 musicStateLabel(snap.state), displayIndex, snap.playlistCount);
    } else {
        snprintf(infoLine, sizeof(infoLine), "%s", musicStateLabel(snap.state));
    }
    gui_.setFont(kEnglishUiFont);
    gui_.drawText(innerX, kBottomY + 48, infoLine, ColorBlack, ColorWhite);

    const int barX = innerX;
    const int barY = kBottomY + 76;
    const int barW = innerW;
    gui_.drawLine(barX, barY, barX + barW, barY, ColorBlack);

    int knobX = barX + barW / 2;
    if (snap.progressKnown && snap.track.durationMs > 0) {
        knobX = barX + static_cast<int>((static_cast<uint64_t>(barW) * snap.progressMs) / snap.track.durationMs);
        if (knobX < barX) knobX = barX;
        if (knobX > barX + barW) knobX = barX + barW;
    }
    gui_.fillCircle(knobX, barY, 3, ColorBlack);

    const int iconY = kBottomY + 94;
    const int leftIconX = kMusicCardX + 36;
    const int centerIconX = kMusicCardX + kMusicCardW / 2 - Mp3ControlIcons::kIconW / 2;
    const int rightIconX = kMusicCardX + kMusicCardW - 36 - Mp3ControlIcons::kIconW;
    gui_.drawBitmap(leftIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    Mp3ControlIcons::kBackward, ColorBlack);
    gui_.drawBitmap(centerIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    snap.state == Mp3Controller::State::PLAYING ? Mp3ControlIcons::kPause
                                                                : Mp3ControlIcons::kPlay,
                    ColorBlack);
    gui_.drawBitmap(rightIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    Mp3ControlIcons::kForward, ColorBlack);

    if (focus_ == Focus::MUSIC) {
        drawFocusRings(kMusicCardX, kBottomY, kMusicCardW, kBottomH, ColorBlack);
    }
}

LaunchState::Focus LaunchState::nextFocus(Focus focus) const {
    switch (focus) {
    case Focus::TIME:  return Focus::DATE;
    case Focus::DATE:  return Focus::INFO;
    case Focus::INFO:  return Focus::MUSIC;
    case Focus::MUSIC: return Focus::TIME;
    }
    return Focus::TIME;
}

void LaunchState::drawCardByFocus(Focus focus) {
    switch (focus) {
    case Focus::TIME:
        drawTimeCard();
        break;
    case Focus::DATE:
        drawDateCard();
        break;
    case Focus::INFO:
        drawInfoCard();
        break;
    case Focus::MUSIC:
        drawMusicCard();
        break;
    }
}

void LaunchState::startTimeAnimation(uint8_t oldHour, uint8_t oldMinute, uint8_t newHour, uint8_t newMinute) {
    displayedTime_.hour = newHour;
    displayedTime_.minute = newMinute;
    displayedTime_.second = currentTime_.second;

    formatTimeDigits(timeAnim_.fromDigits, oldHour, oldMinute);
    formatTimeDigits(timeAnim_.toDigits, newHour, newMinute);
    timeAnim_.startMs = millis();
    timeAnim_.active = true;

    drawTimeDigitsAnimated(kTimeCardX, kTimeDigitsY, 0);
}

void LaunchState::finishTimeAnimation() {
    timeAnim_.active = false;
    displayedTime_ = currentTime_;
    drawTimeDigits(kTimeCardX, kTimeDigitsY, displayedTime_.hour, displayedTime_.minute);
}
