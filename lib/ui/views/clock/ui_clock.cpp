#include "ui_clock.h"
#include <ui/gui/Gui.h>
#include <device/display/display_bsp.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18.h>

extern Gui gui;

// ── 时间显示参数（居中，大号 72x96）──────────────────────────
static const int kTimeCharW  = 72;
static const int kTimeCharH  = 96;
static const int kTimeStrLen = 5;   // "HH:MM"
static const int kTimeX = (400 - kTimeStrLen * kTimeCharW) / 2;
static const int kTimeY = (300 - kTimeCharH) / 2;

// ── 日期/星期显示参数（右上角，同一行）──────────────────────────
static const int kDateY           = 8;
static const int kDateRightMargin = 10;

// 星期名称（UTF-8，周X 格式，与 18x18 字体字符集匹配）
static const char *kWeekNames[7] = {
    "\xe5\x91\xa8\xe6\x97\xa5",  // 周日
    "\xe5\x91\xa8\xe4\xb8\x80",  // 周一
    "\xe5\x91\xa8\xe4\xba\x8c",  // 周二
    "\xe5\x91\xa8\xe4\xb8\x89",  // 周三
    "\xe5\x91\xa8\xe5\x9b\x9b",  // 周四
    "\xe5\x91\xa8\xe4\xba\x94",  // 周五
    "\xe5\x91\xa8\xe5\x85\xad",  // 周六
};

static uint8_t s_lastMinute = 0xFF;
static uint8_t s_lastDay    = 0xFF;

void drawTime(const RTCTime &t) {
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.hour, t.minute);
    gui.fillRect(kTimeX, kTimeY, kTimeStrLen * kTimeCharW, kTimeCharH, ColorWhite);
    gui.setFont(&kFont_Alibaba72x96);
    gui.drawText(kTimeX, kTimeY, timeBuf);
}

void drawDateWeek(const RTCTime &t) {
    char dateBuf[8];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d", t.month, t.day);

    // 同一行：[周X] [MM/DD]，右对齐
    static const int kDateCharW = 12;  // 12×18 date font advance
    static const int kWeekCharW = 18;  // 18×18 weekday font advance
    static const int kGap       = 6;   // gap between weekday and date

    int dateW = 5 * kDateCharW;        // "MM/DD" = 5 chars = 60px
    int weekW = 2 * kWeekCharW;        // "周X"   = 2 chars = 36px
    int totalW = weekW + kGap + dateW; // 102px total

    int clearX = 400 - totalW - kDateRightMargin;
    int clearH = 18 + 4;
    gui.fillRect(clearX - 2, kDateY - 2, totalW + 4, clearH, ColorWhite);

    int dateX = clearX;
    int weekX = dateX + dateW + kGap;

    gui.setFont(&kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18);
    gui.drawText(dateX, kDateY, dateBuf, ColorBlack, ColorWhite);

    uint8_t wd = t.weekday < 7 ? t.weekday : 0;
    gui.setFont(&kFont18_AlibabaPuHuiTi_3_75_SemiBold);
    gui.drawText(weekX, kDateY, kWeekNames[wd], ColorBlack, ColorWhite);
}

void handleRtcUpdate(const RTCTime &t) {
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
    s_lastDay    = 0xFF;
}
