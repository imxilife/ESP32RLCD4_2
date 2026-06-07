#include "ui_clock.h"

#include <device/display/display_bsp.h>
#include <cstdio>
#include <ui/gui/Gui.h>
#include <ui/gui/fonts/FontManager.h>

extern Gui gui;

namespace {

constexpr int kScreenW = 400;
constexpr int kDateY = 8;
constexpr int kDateRightMargin = 10;
constexpr int kTimeY = 92;
constexpr int kColonW = 18;

const char* kWeekNames[7] = {
    "\xe5\x91\xa8\xe6\x97\xa5",
    "\xe5\x91\xa8\xe4\xb8\x80",
    "\xe5\x91\xa8\xe4\xba\x8c",
    "\xe5\x91\xa8\xe4\xb8\x89",
    "\xe5\x91\xa8\xe5\x9b\x9b",
    "\xe5\x91\xa8\xe4\xba\x94",
    "\xe5\x91\xa8\xe5\x85\xad",
};

uint8_t s_lastMinute = 0xFF;
uint8_t s_lastDay = 0xFF;

}  // namespace

void drawTime(const RTCTime& t) {
    const Font* font = FontManager::instance().font(FontId::Digits60);
    if (font == nullptr) return;

    char hourBuf[4];
    char minuteBuf[4];
    snprintf(hourBuf, sizeof(hourBuf), "%02d", t.hour);
    snprintf(minuteBuf, sizeof(minuteBuf), "%02d", t.minute);

    // The project digit bin contains digits only, so the colon remains a primitive.
    const int hourW = gui.measureTextWidth(hourBuf, font);
    const int minuteW = gui.measureTextWidth(minuteBuf, font);
    const int totalW = hourW + kColonW + minuteW;
    int x = (kScreenW - totalW) / 2;
    if (x < 0) x = 0;

    gui.fillRect(0, kTimeY - 4, kScreenW, font->lineHeight + 8, ColorWhite);
    gui.setFont(font);
    gui.drawText(x, kTimeY, hourBuf, ColorBlack, ColorWhite);

    const int colonX = x + hourW + kColonW / 2;
    gui.fillCircle(colonX, kTimeY + font->lineHeight * 34 / 100, 4, ColorBlack);
    gui.fillCircle(colonX, kTimeY + font->lineHeight * 66 / 100, 4, ColorBlack);

    gui.drawText(x + hourW + kColonW, kTimeY, minuteBuf, ColorBlack, ColorWhite);
}

void drawDateWeek(const RTCTime& t) {
    const Font* dateFont = FontManager::instance().font(FontId::EnSub);
    const Font* weekFont = FontManager::instance().font(FontId::ZhSub);
    if (dateFont == nullptr || weekFont == nullptr) return;

    char dateBuf[8];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", t.month, t.day);

    constexpr int kGap = 6;
    const uint8_t wd = t.weekday < 7 ? t.weekday : 0;
    const int dateW = gui.measureTextWidth(dateBuf, dateFont);
    const int weekW = gui.measureTextWidth(kWeekNames[wd], weekFont);
    const int totalW = weekW + kGap + dateW;

    int clearX = kScreenW - totalW - kDateRightMargin;
    if (clearX < 0) clearX = 0;
    gui.fillRect(clearX - 2, kDateY - 2, totalW + 4, 28, ColorWhite);

    const int dateX = clearX;
    const int weekX = dateX + dateW + kGap;

    gui.setFont(dateFont);
    gui.drawText(dateX, kDateY, dateBuf, ColorBlack, ColorWhite);
    gui.setFont(weekFont);
    gui.drawText(weekX, kDateY, kWeekNames[wd], ColorBlack, ColorWhite);
}

void handleRtcUpdate(const RTCTime& t) {
    if (t.minute != s_lastMinute) {
        drawTime(t);
        s_lastMinute = t.minute;
    }
    if (t.day != s_lastDay) {
        drawDateWeek(t);
        s_lastDay = t.day;
    }
}

void resetClockState() {
    s_lastMinute = 0xFF;
    s_lastDay = 0xFF;
}
