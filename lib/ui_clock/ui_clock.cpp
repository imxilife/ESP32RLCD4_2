#include "ui_clock.h"
#include <Gui.h>
#include <display_bsp.h>
#include "fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h"
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18.h"
#include "fonts/Font_ascii_Geom_Bold_20_20.h"

extern Gui gui;

// ── 时间显示参数（居中，大号 72x96）──────────────────────────
static const int kTimeCharW  = 72;
static const int kTimeCharH  = 96;
static const int kTimeStrLen = 5;   // "HH:MM"
static const int kTimeX = (400 - kTimeStrLen * kTimeCharW) / 2;
static const int kTimeY = (300 - kTimeCharH) / 2;

// ── 日期/星期显示参数（右上角）────────────────────────────────
static const int kDateY           = 8;
static const int kWeekY           = kDateY + 32 + 4;
static const int kDateRightMargin = 10;

// 星期名称（UTF-8）
static const char *kWeekNames[7] = {
    "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5",  // 星期日
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",  // 星期一
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c",  // 星期二
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89",  // 星期三
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b",  // 星期四
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",  // 星期五
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad",  // 星期六
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

    int dateW = 5 * 24;
    int dateX = 400 - dateW - kDateRightMargin;

    int weekW = 3 * 16;
    int weekX = 400 - weekW - kDateRightMargin;

    int clearW = (dateW > weekW) ? dateW : weekW;
    int clearX = 400 - clearW - kDateRightMargin;
    int clearH = 32 + 4 + 16 + 4;
    gui.fillRect(clearX, kDateY - 2, clearW + 2, clearH, ColorWhite);

    gui.setFont(&kFont_ascii_Geom_Bold_20_20);
    gui.drawText(dateX, kDateY, dateBuf, ColorBlack, ColorWhite);

    uint8_t wd = t.weekday < 7 ? t.weekday : 0;
    gui.setFont(&kFont18_AlibabaPuHuiTi_3_75_SemiBold);
    gui.drawText(weekX, kWeekY, kWeekNames[wd], ColorBlack, ColorWhite);
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
