#include "ui_pomodoro.h"

#include <device/display/display_bsp.h>
#include <math.h>

#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font_ascii_Oswald_Light_28_40.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec.h>

// ── 布局：设置界面 ────────────────────────────────────────────
static const int kSetupTitleY     = 16;
static const int kSetupTimeY      = 90;
static const int kSetupBtnTextY   = 244;
static const int kSetupBtnBorderY = 236;
static const int kSetupBtnH       = 36;
static const int kSetupBtnPad     = 8;
static const int kNumBgPad        = 8;

// ── 布局：倒计时圆环 ──────────────────────────────────────────
static const int   kRingCX      = 200;
static const int   kRingCY      = 150;
static const int   kRingInnerR  = 117;
static const int   kRingInner5R = 109;
static const int   kRingOuterR  = 125;
static const int   kRingTicks   = 60;

// ── 字体尺寸常量 ──────────────────────────────────────────────
static const int kBigW = 72;
static const int kBigH = 96;
static const int kSmW  = 20;
static const int kSmH  = 20;
static const int kOsW  = 28;
static const int kOsH  = 40;

// ─────────────────────────────────────────────────────────────

static void drawProgressRing(Gui& gui, uint32_t remSec, uint32_t totalSec) {
    int filled = (totalSec > 0)
                 ? (int)((uint64_t)remSec * kRingTicks / totalSec)
                 : 0;
    if (filled > kRingTicks) filled = kRingTicks;

    for (int i = 0; i < kRingTicks; i++) {
        float angle = -(float)M_PI_2 + (2.0f * (float)M_PI * i) / kRingTicks;
        float ca = cosf(angle);
        float sa = sinf(angle);

        bool isMark = (i % 5 == 0);
        int  innerR = isMark ? kRingInner5R : kRingInnerR;

        int x1 = kRingCX + (int)(innerR      * ca);
        int y1 = kRingCY + (int)(innerR      * sa);
        int x2 = kRingCX + (int)(kRingOuterR * ca);
        int y2 = kRingCY + (int)(kRingOuterR * sa);

        uint8_t color = (i < filled) ? ColorBlack : ColorWhite;
        int px = (fabsf(sa) >= fabsf(ca)) ? 1 : 0;
        int py = (fabsf(sa) >= fabsf(ca)) ? 0 : 1;
        gui.drawLine(x1,    y1,    x2,    y2,    color);
        gui.drawLine(x1+px, y1+py, x2+px, y2+py, color);
    }
}

static void drawTimeLarge(Gui& gui, uint32_t remSec) {
    char sBuf[3];
    snprintf(sBuf, sizeof(sBuf), "%02d", (int)remSec);
    int totalH = kOsH + 8 + kSmH;
    int ty = 150 - totalH / 2;
    int tx = 200 - kOsW;
    gui.setFont(&kFont_ascii_Oswald_Light_28_40);
    gui.fillRect(tx, ty, 2 * kOsW, kOsH, ColorWhite);
    gui.drawText(tx, ty, sBuf, ColorBlack, ColorWhite);
    int secX = 200 - (3 * kSmW) / 2;
    int secY = ty + kOsH + 8;
    gui.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    secX = 200 - gui.measureTextWidth("sec", &kFont_ascii_IBMPlexSans_Medium_20_20) / 2;
    gui.fillRect(secX, secY, gui.measureTextWidth("sec", &kFont_ascii_IBMPlexSans_Medium_20_20), kSmH, ColorWhite);
    gui.drawText(secX, secY, "sec", ColorBlack, ColorWhite);
}

static void drawTimeSmall(Gui& gui, const char* fmtStr) {
    int x = 200 - (5 * kOsW) / 2;
    int y = 150 - kOsH / 2;
    gui.setFont(&kFont_ascii_Oswald_Light_28_40);
    gui.fillRect(x, y, 5 * kOsW, kOsH, ColorWhite);
    gui.drawText(x, y, fmtStr, ColorBlack, ColorWhite);
}

static void drawSetup(Gui& gui, uint8_t setH, uint8_t setM, uint8_t focus) {
    gui.clear();
    gui.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    gui.drawText((400 - gui.measureTextWidth("POMODORO", &kFont_ascii_IBMPlexSans_Medium_20_20)) / 2,
                 kSetupTitleY, "POMODORO", ColorBlack, ColorWhite);

    static const int kTX = (400 - 5 * kBigW) / 2;
    static const int kTY = kSetupTimeY;

    char hBuf[3], mBuf[3];
    snprintf(hBuf, sizeof(hBuf), "%02d", setH);
    snprintf(mBuf, sizeof(mBuf), "%02d", setM);

    bool hFocus = (focus == 0);  // Focus::HOURS
    bool mFocus = (focus == 1);  // Focus::MINUTES

    gui.setFont(&kFont_Alibaba72x96);

    // 分钟段（左侧）
    {
        int bgX  = kTX - kNumBgPad;
        int bgY  = kTY - kNumBgPad;
        int bgW  = 2 * kBigW + kNumBgPad;
        int bgH  = kBigH + 2 * kNumBgPad;
        int txtX = kTX - kNumBgPad / 2;
        if (hFocus) {
            gui.fillRoundRect(bgX, bgY, bgW, bgH, 8, ColorBlack);
            gui.drawText(txtX, kTY, hBuf, ColorWhite, ColorTransparent);
        } else {
            gui.fillRect(bgX, bgY, bgW, bgH, ColorWhite);
            gui.drawText(txtX, kTY, hBuf, ColorBlack, ColorWhite);
        }
    }

    // 冒号
    {
        static const int kColonX  = kTX + 2 * kBigW + kBigW / 2;
        static const int kColonR  = 7;
        static const int kColonY1 = kTY + kBigH * 30 / 100;
        static const int kColonY2 = kTY + kBigH * 70 / 100;
        gui.fillRect(kTX + 2 * kBigW, kTY - kNumBgPad, kBigW, kBigH + 2 * kNumBgPad, ColorWhite);
        gui.fillCircle(kColonX, kColonY1, kColonR, ColorBlack);
        gui.fillCircle(kColonX, kColonY2, kColonR, ColorBlack);
    }

    // 秒钟段（右侧）
    {
        int secX = kTX + 3 * kBigW;
        int bgX  = secX;
        int bgY  = kTY - kNumBgPad;
        int bgW  = 2 * kBigW + kNumBgPad;
        int bgH  = kBigH + 2 * kNumBgPad;
        int txtX = secX + kNumBgPad / 2;
        if (mFocus) {
            gui.fillRoundRect(bgX, bgY, bgW, bgH, 8, ColorBlack);
            gui.drawText(txtX, kTY, mBuf, ColorWhite, ColorTransparent);
        } else {
            gui.fillRect(bgX, bgY, bgW, bgH, ColorWhite);
            gui.drawText(txtX, kTY, mBuf, ColorBlack, ColorWhite);
        }
    }

    // 标签 "分" / "秒"
    {
        static const int kLabelY = kSetupTimeY + kBigH + kNumBgPad + 4;
        gui.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec);
        gui.drawText(kTX + kBigW - kSmW / 2,          kLabelY,
                     "\xe5\x88\x86", ColorBlack, ColorWhite);  // 分
        gui.drawText(kTX + 3 * kBigW + kBigW - kSmW / 2, kLabelY,
                     "\xe7\xa7\x92", ColorBlack, ColorWhite);  // 秒
    }

    // 按钮行
    gui.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);

    // EXIT
    {
        static const int kW  = 4 * kSmW;
        const int kTx = 100 - gui.measureTextWidth("EXIT", &kFont_ascii_IBMPlexSans_Medium_20_20) / 2;
        bool focused = (focus == 2);  // Focus::EXIT
        int rx = kTx - kSetupBtnPad;
        int rw = kW  + 2 * kSetupBtnPad;
        if (focused) {
            gui.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        } else {
            gui.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorWhite);
            gui.drawRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        }
        gui.drawText(kTx, kSetupBtnTextY, "EXIT",
                     focused ? ColorWhite : ColorBlack,
                     focused ? ColorBlack : ColorWhite);
    }

    // START
    {
        static const int kW  = 5 * kSmW;
        const int kTx = 300 - gui.measureTextWidth("START", &kFont_ascii_IBMPlexSans_Medium_20_20) / 2;
        bool focused = (focus == 3);  // Focus::START
        int rx = kTx - kSetupBtnPad;
        int rw = kW  + 2 * kSetupBtnPad;
        if (focused) {
            gui.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        } else {
            gui.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorWhite);
            gui.drawRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        }
        gui.drawText(kTx, kSetupBtnTextY, "START",
                     focused ? ColorWhite : ColorBlack,
                     focused ? ColorBlack : ColorWhite);
    }
}

static void drawCountdown(Gui& gui, uint32_t remSec, uint32_t totalSec) {
    gui.fillRect(0, 0, 400, 300, ColorWhite);
    drawProgressRing(gui, remSec, totalSec);
    if (remSec <= 60) {
        drawTimeLarge(gui, remSec);
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", (int)(remSec / 60), (int)(remSec % 60));
        drawTimeSmall(gui, buf);
    }
}

static void drawFinished(Gui& gui, bool blinkOn) {
    uint8_t bg = blinkOn ? ColorWhite : ColorBlack;
    uint8_t fg = blinkOn ? ColorBlack : ColorWhite;
    gui.fillRect(0, 0, 400, 300, bg);
    gui.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    gui.drawText((400 - gui.measureTextWidth("DONE! HOLD KEY1", &kFont_ascii_IBMPlexSans_Medium_20_20)) / 2,
                 (300 - kSmH) / 2, "DONE! HOLD KEY1", fg, bg);
}

// ── 公共入口 ─────────────────────────────────────────────────

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
        // main.cpp 收到后负责 gui.clear() + resetClockState()
        break;
    }
}
