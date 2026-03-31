#pragma once

#include <core/state_manager/AbstractState.h>
#include <device/rtc/RTC85063.h>
#include <ui/gui/Font.h>
#include <ui/gui/Gui.h>

class LaunchState : public AbstractState {
public:
    explicit LaunchState(Gui& gui);

    void onEnter() override;
    void onExit() override;
    void onMessage(const AppMessage& msg) override;
    void onKeyEvent(const KeyEvent& event) override;
    void tick() override;

private:
    enum class Focus : uint8_t {
        TIME = 0,
        DATE = 1,
        INFO = 2,
        MUSIC = 3,
    };

    Gui& gui_;
    RTC85063 rtc_;
    RTCTime currentTime_ = {};
    RTCTime displayedTime_ = {};
    Focus focus_ = Focus::TIME;
    bool hasRtc_ = false;

    struct TimeScrollAnimation {
        bool active = false;
        uint32_t startMs = 0;
        uint16_t durationMs = 220;
        char fromDigits[5] = {'0', '0', '0', '0', '\0'};
        char toDigits[5] = {'0', '0', '0', '0', '\0'};
    };

    TimeScrollAnimation timeAnim_;

    static constexpr int kScreenW = 400;
    static constexpr int kScreenH = 300;
    static constexpr int kPageMargin = 12;
    static constexpr int kGap = 10;
    static constexpr int kTopY = 18;
    static constexpr int kTopH = 126;
    static constexpr int kBottomY = kTopY + kTopH + kGap;
    static constexpr int kBottomH = 136;
    static constexpr int kTimeCardX = kPageMargin;
    static constexpr int kTimeCardW = 266;
    static constexpr int kDateCardX = kTimeCardX + kTimeCardW + kGap;
    static constexpr int kDateCardW = kScreenW - kPageMargin - kDateCardX;
    static constexpr int kInfoCardX = kPageMargin;
    static constexpr int kInfoCardW = 140;
    static constexpr int kMusicCardX = kInfoCardX + kInfoCardW + kGap;
    static constexpr int kMusicCardW = kScreenW - kPageMargin - kMusicCardX;
    static constexpr int kCardRadius = 10;
    static constexpr int kFocusInset1 = 2;
    static constexpr int kFocusInset2 = 4;
    static constexpr int kTimeDigitW = 40;
    static constexpr int kTimeDigitH = 64;
    static constexpr int kTimeDigitGap = 4;
    static constexpr int kTimeGroupGap = 26;
    static constexpr int kTimeColonRadius = 4;
    static constexpr int kTimeColonGap = 10;
    static constexpr int kTimeDigitsY = kTopY + 30;
    static constexpr int kDateWeekRadius = 4;
    static constexpr int kDateSplitGap = 2;
    static constexpr int kDateSplitOffsetY = 19;

    void drawFrame();
    void draw();
    void redrawFocus(Focus previous, Focus current);
    void drawCard(int x, int y, int w, int h);
    void drawFocusRings(int x, int y, int w, int h, uint8_t color);
    void drawFocusByState(Focus focus, bool focused);
    void drawCenteredText(int x, int y, int w, const char* text, const Font* font,
                          uint8_t fg, uint8_t bg);
    void drawCenteredMeasuredText(int x, int y, int w, const char* text, const Font* font,
                                  uint8_t fg, uint8_t bg);
    void drawTimeDigits(int x, int y, uint8_t hour, uint8_t minute);
    void drawTimeDigitsAnimated(int x, int y, uint8_t offset);
    void drawTimeCard();
    void drawDateCard();
    void drawInfoCard();
    void drawMusicCard();
    void drawCardByFocus(Focus focus);
    Focus nextFocus(Focus focus) const;

    void formatTimeDigits(char out[5], uint8_t hour, uint8_t minute) const;
    int timeDigitsStartX(int x) const;
    int timeDigitX(int cardX, int index) const;
    void drawTimeColon(int x, int y);
    void drawTimeDigitCell(int x, int y, char digit);
    void drawDigitGlyphClipped(int x, int y,
                               const uint8_t* glyph, int glyphW, int glyphH, int stride,
                               int clipX, int clipY, int clipW, int clipH);
    void startTimeAnimation(uint8_t oldHour, uint8_t oldMinute, uint8_t newHour, uint8_t newMinute);
    void finishTimeAnimation();
};
