#include "Pomodoro.h"

#include <Gui.h>
#include <display_bsp.h>
#include <math.h>
#include <stdio.h>

#include "fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96.h"
#include "fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"
#include "fonts/Font_ascii_Oswald_Light_28_40.h"
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec.h"

// ── 布局：设置界面 ────────────────────────────────────────────────
static const int kSetupTitleY     = 16;    // 标题 Y
static const int kSetupTimeY      = 90;    // HH:MM 大字 Y
static const int kSetupBtnTextY   = 244;   // 按钮文字 Y
static const int kSetupBtnBorderY = 236;   // 按钮边框 Y
static const int kSetupBtnH       = 36;    // 按钮边框高
static const int kSetupBtnPad     = 8;     // 按钮内边距
static const int kNumBgPad        = 8;     // 获焦数字背景内边距

// ── 布局：倒计时圆环 ──────────────────────────────────────────────
static const int   kRingCX      = 200;
static const int   kRingCY      = 150;
static const int   kRingInnerR  = 117;   // 普通刻度内半径（短刻度 8px）
static const int   kRingInner5R = 109;   // 每 5 格长刻度内半径（长刻度 16px）
static const int   kRingOuterR  = 125;   // 刻度外半径
static const int   kRingTicks   = 60;

// ── 字体尺寸常量 ──────────────────────────────────────────────────
static const int kBigW = 72;   // 72×96 字体每字符宽
static const int kBigH = 96;
static const int kSmW  = 20;   // 20×20 字体每字符宽
static const int kSmH  = 20;
static const int kOsW  = 28;   // Oswald 28×40 每字符宽
static const int kOsH  = 40;   // Oswald 28×40 每字符高

// ── 长按快速累加间隔 ──────────────────────────────────────────────
static const uint32_t kFastRepeatMs = 120;  // 长按时每次累加间隔

// ─────────────────────────────────────────────────────────────────

Pomodoro::Pomodoro(Gui &gui) : gui_(gui) {}

void Pomodoro::begin() {
    pinMode(KEY1_PIN, INPUT_PULLUP);
    pinMode(KEY2_PIN, INPUT_PULLUP);
    // 用真实 GPIO 状态初始化，防止启动时产生虚假边沿
    key1Prev_ = (digitalRead(KEY1_PIN) == LOW);
    key2Prev_ = (digitalRead(KEY2_PIN) == LOW);
    Serial.printf("[Pomodoro] begin: KEY1(GPIO%d)=%s  KEY2(GPIO%d)=%s\n",
                  KEY1_PIN, key1Prev_ ? "LOW" : "HIGH",
                  KEY2_PIN, key2Prev_ ? "LOW" : "HIGH");
}

// ── 主循环入口 ────────────────────────────────────────────────────

void Pomodoro::update() {
    pollButtons();

    switch (state_) {
    case State::IDLE:
        break;

    case State::SETUP:
        // 所有绘制在按键事件中触发，此处无需轮询
        break;

    case State::COUNTDOWN: {
        uint32_t now = millis();
        if (now - lastTickMs_ >= 1000) {
            lastTickMs_ = now;
            if (remSec_ > 0) {
                remSec_--;
                drawCountdown();
            } else {
                enterFinished();
            }
        }
        break;
    }

    case State::FINISHED: {
        uint32_t now = millis();
        if (now - lastBlinkMs_ >= 500) {
            lastBlinkMs_ = now;
            blinkOn_ = !blinkOn_;
            drawFinished();
        }
        break;
    }
    }
}

// ── 按键轮询 ─────────────────────────────────────────────────────

void Pomodoro::pollButtons() {
    uint32_t now = millis();

    // KEY1：下降沿触发
    bool k1 = (digitalRead(KEY1_PIN) == LOW);
    if (k1 && !key1Prev_) {
        onKey1();
    }
    key1Prev_ = k1;

    // KEY2：短按执行一次，长按（≥ LONG_PRESS_MS）后每 kFastRepeatMs 重复一次
    bool k2 = (digitalRead(KEY2_PIN) == LOW);
    if (k2 && !key2Prev_) {
        // 按下：记录时间，重置长按标志
        key2PressMs_   = now;
        key2LongFired_ = false;
    }
    if (!k2 && key2Prev_ && !key2LongFired_) {
        // 释放且尚未触发长按 → 短按
        onKey2Short();
    }
    if (k2 && (now - key2PressMs_ >= LONG_PRESS_MS)) {
        if (!key2LongFired_) {
            // 首次进入长按：立即执行一次并开始计时
            key2LongFired_ = true;
            key2RepeatMs_  = now;
            onKey2Short();
        } else if (now - key2RepeatMs_ >= kFastRepeatMs) {
            // 持续长按：按节拍重复
            key2RepeatMs_ = now;
            onKey2Short();
        }
    }
    key2Prev_ = k2;
}

// ── 按键事件 ──────────────────────────────────────────────────────

void Pomodoro::onKey1() {
    switch (state_) {
    case State::IDLE:
        enterSetup();
        break;

    case State::SETUP:
        // KEY1 只循环切换焦点，不执行任何动作
        switch (focus_) {
        case Focus::HOURS:
            focus_ = Focus::MINUTES;
            drawSetup();
            break;
        case Focus::MINUTES:
            focus_ = Focus::EXIT;
            drawSetup();
            break;
        case Focus::EXIT:
            focus_ = Focus::START;
            drawSetup();
            break;
        case Focus::START:
            focus_ = Focus::HOURS;
            drawSetup();
            break;
        }
        break;

    case State::COUNTDOWN:
        enterIdle();
        break;

    case State::FINISHED:
        enterIdle();
        break;
    }
}

void Pomodoro::onKey2Short() {
    if (state_ != State::SETUP) return;
    switch (focus_) {
    case Focus::HOURS:
        setH_ = (setH_ + 1) % 100;   // 超过 99 循环回 0
        drawSetup();
        break;
    case Focus::MINUTES:
        setM_ = (setM_ + 1) % 100;
        drawSetup();
        break;
    case Focus::EXIT:
        enterIdle();
        break;
    case Focus::START:
        enterCountdown();
        break;
    }
}

// ── 状态切换 ─────────────────────────────────────────────────────

void Pomodoro::enterSetup() {
    state_ = State::SETUP;
    focus_ = Focus::HOURS;
    setH_  = 25;    // 分钟
    setM_  = 0;   // 秒钟
    drawSetup();
}

void Pomodoro::enterCountdown() {
    uint32_t t = (uint32_t)setH_ * 60u + (uint32_t)setM_;
    if (t == 0) {
        // 时长为零，退回分钟编辑
        focus_ = Focus::MINUTES;
        drawSetup();
        return;
    }
    totalSec_   = t;
    remSec_     = t;
    lastTickMs_ = millis();
    state_      = State::COUNTDOWN;
    drawCountdown();
}

void Pomodoro::enterFinished() {
    state_       = State::FINISHED;
    blinkOn_     = true;
    lastBlinkMs_ = millis();
    drawFinished();
}

void Pomodoro::enterIdle() {
    state_ = State::IDLE;
    // main.cpp 检测到 isActive() 由 true→false 时负责 gui.clear() + resetClockState()
}

// ── 绘制：设置界面 ────────────────────────────────────────────────

void Pomodoro::drawSetup() {
    gui_.clear();  // 白底

    // 标题 "POMODORO"（居中）
    gui_.setFont(&kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20);
    gui_.drawText((400 - 8 * kSmW) / 2, kSetupTitleY, "POMODORO",
                  ColorBlack, ColorWhite);

    // ── HH : MM（大号 72×96，焦点字段反色）────────────────────────
    static const int kTX = (400 - 5 * kBigW) / 2;   // = 20
    static const int kTY = kSetupTimeY;

    char hBuf[3], mBuf[3];
    snprintf(hBuf, sizeof(hBuf), "%02d", setH_);
    snprintf(mBuf, sizeof(mBuf), "%02d", setM_);

    bool hFocus = (focus_ == Focus::HOURS);
    bool mFocus = (focus_ == Focus::MINUTES);

    gui_.setFont(&kFont_Alibaba72x96);

    // ── 数字段：内边距只向外侧扩展，内侧（靠冒号一边）不扩展，避免覆盖冒号区域 ────
    // 分钟段（左侧）：左/上/下加 kNumBgPad，右边缘 = kTX+2*kBigW（与冒号区左边界齐）
    {
        int bgX = kTX - kNumBgPad;
        int bgY = kTY - kNumBgPad;
        int bgW = 2 * kBigW + kNumBgPad;   // 只有左侧内边距
        int bgH = kBigH + 2 * kNumBgPad;
        int txtX = kTX - kNumBgPad / 2;   // 向左偏移 4px，使文字在背景中居中
        if (hFocus) {
            gui_.fillRoundRect(bgX, bgY, bgW, bgH, 8, ColorBlack);
            gui_.drawText(txtX, kTY, hBuf, ColorWhite, ColorTransparent);
        } else {
            gui_.fillRect(bgX, bgY, bgW, bgH, ColorWhite);
            gui_.drawText(txtX, kTY, hBuf, ColorBlack, ColorWhite);
        }
    }

    // 冒号：两个 fillCircle，不用字模，彻底无背景填充冲突
    {
        static const int kColonX  = kTX + 2 * kBigW + kBigW / 2;   // 200
        static const int kColonR  = 7;
        static const int kColonY1 = kTY + kBigH * 30 / 100;         // ≈ 119
        static const int kColonY2 = kTY + kBigH * 70 / 100;         // ≈ 157
        // 清除冒号区（上下含内边距高度，X 从 kTX+2*kBigW 开始，与"00"背景右边界对齐，无重叠）
        gui_.fillRect(kTX + 2 * kBigW, kTY - kNumBgPad,
                      kBigW, kBigH + 2 * kNumBgPad, ColorWhite);
        gui_.fillCircle(kColonX, kColonY1, kColonR, ColorBlack);
        gui_.fillCircle(kColonX, kColonY2, kColonR, ColorBlack);
    }

    // 秒钟段（右侧）：右/上/下加 kNumBgPad，左边缘 = kTX+3*kBigW（与冒号区右边界齐）
    {
        int secX = kTX + 3 * kBigW;
        int bgX = secX;                    // 左边缘不扩展
        int bgY = kTY - kNumBgPad;
        int bgW = 2 * kBigW + kNumBgPad;   // 只有右侧内边距
        int bgH = kBigH + 2 * kNumBgPad;
        int txtX = secX + kNumBgPad / 2;   // 向右偏移 4px，使文字在背景中居中
        if (mFocus) {
            gui_.fillRoundRect(bgX, bgY, bgW, bgH, 8, ColorBlack);
            gui_.drawText(txtX, kTY, mBuf, ColorWhite, ColorTransparent);
        } else {
            gui_.fillRect(bgX, bgY, bgW, bgH, ColorWhite);
            gui_.drawText(txtX, kTY, mBuf, ColorBlack, ColorWhite);
        }
    }

    // ── "分" / "秒" 标签（数字下方，与各段水平居中）───────────────
    {
        static const int kLabelY = kSetupTimeY + kBigH + kNumBgPad + 4;
        gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20_minsec);
        // "分" 居中于左侧数字组（x=kTX, width=2*kBigW）
        int minX = kTX + kBigW - kSmW / 2;
        gui_.drawText(minX, kLabelY, "\xe5\x88\x86", ColorBlack, ColorWhite); // 分 U+5206
        // "秒" 居中于右侧数字组（x=kTX+3*kBigW, width=2*kBigW）
        int secLX = kTX + 3 * kBigW + kBigW - kSmW / 2;
        gui_.drawText(secLX, kLabelY, "\xe7\xa7\x92", ColorBlack, ColorWhite); // 秒 U+79D2
    }

    // ── 按钮行：[EXIT]（居中于 x=100）  [START]（居中于 x=300）──
    gui_.setFont(&kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    // EXIT 按钮
    {
        static const int kW  = 4 * kSmW;               // "EXIT"  80px
        static const int kTx = 100 - kW / 2;           // = 60
        bool focused = (focus_ == Focus::EXIT);
        int  rx = kTx - kSetupBtnPad;
        int  rw = kW  + 2 * kSetupBtnPad;
        if (focused) {
            gui_.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        } else {
            gui_.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorWhite);
            gui_.drawRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        }
        gui_.drawText(kTx, kSetupBtnTextY, "EXIT",
                      focused ? ColorWhite : ColorBlack,
                      focused ? ColorBlack : ColorWhite);
    }

    // START 按钮
    {
        static const int kW  = 5 * kSmW;               // "START" 100px
        static const int kTx = 300 - kW / 2;           // = 250
        bool focused = (focus_ == Focus::START);
        int  rx = kTx - kSetupBtnPad;
        int  rw = kW  + 2 * kSetupBtnPad;
        if (focused) {
            gui_.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        } else {
            gui_.fillRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorWhite);
            gui_.drawRoundRect(rx, kSetupBtnBorderY, rw, kSetupBtnH, 4, ColorBlack);
        }
        gui_.drawText(kTx, kSetupBtnTextY, "START",
                      focused ? ColorWhite : ColorBlack,
                      focused ? ColorBlack : ColorWhite);
    }
}

// ── 绘制：倒计时 ─────────────────────────────────────────────────

void Pomodoro::drawCountdown() {
    gui_.fillRect(0, 0, 400, 300, ColorWhite);  // 白底
    drawProgressRing(remSec_, totalSec_);

    if (remSec_ <= 60) {
        drawTimeLarge(remSec_);
    } else {
        char buf[8];
        // 最大 99分99秒，始终以 MM:SS 显示
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 (int)(remSec_ / 60), (int)(remSec_ % 60));
        drawTimeSmall(buf);
    }
}

// ── 绘制：完成闪烁 ───────────────────────────────────────────────

void Pomodoro::drawFinished() {
    uint8_t bg = blinkOn_ ? ColorWhite : ColorBlack;
    uint8_t fg = blinkOn_ ? ColorBlack : ColorWhite;
    gui_.fillRect(0, 0, 400, 300, bg);
    gui_.setFont(&kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20);
    // "DONE! PRESS KEY1" = 16 chars
    gui_.drawText((400 - 16 * kSmW) / 2, (300 - kSmH) / 2,
                  "DONE! PRESS KEY1", fg, bg);
}

// ── 绘制：圆形进度环 ─────────────────────────────────────────────
// 60 个刻度，从 12 点顺时针排列，白底黑线。
// 每 5 格画一条长刻度（内径更短）；已经过的刻度涂白（隐入背景）。

void Pomodoro::drawProgressRing(uint32_t remSec, uint32_t totalSec) {
    int filled = (totalSec > 0)
                 ? (int)((uint64_t)remSec * kRingTicks / totalSec)
                 : 0;
    if (filled > kRingTicks) filled = kRingTicks;

    for (int i = 0; i < kRingTicks; i++) {
        float angle = -(float)M_PI_2 + (2.0f * (float)M_PI * i) / kRingTicks;
        float ca = cosf(angle);
        float sa = sinf(angle);

        bool isMark = (i % 5 == 0);
        int innerR  = isMark ? kRingInner5R : kRingInnerR;

        int x1 = kRingCX + (int)(innerR      * ca);
        int y1 = kRingCY + (int)(innerR      * sa);
        int x2 = kRingCX + (int)(kRingOuterR * ca);
        int y2 = kRingCY + (int)(kRingOuterR * sa);

        // 顺时针：i < filled 为剩余（黑），i >= filled 为已过（白，隐入白底）
        uint8_t color = (i < filled) ? ColorBlack : ColorWhite;

        // 2px 实心刻度：主法线单次偏移法
        // 近水平线（|ca|>|sa|）→ Y 方向偏移1px；近垂直线（|sa|>=|ca|）→ X 方向偏移1px
        // 保证每个扫描行/列恰好 2px，Bresenham 走阶梯时不产生 3px 凸起，线条无弯折。
        int px = (fabsf(sa) >= fabsf(ca)) ? 1 : 0;
        int py = (fabsf(sa) >= fabsf(ca)) ? 0 : 1;
        gui_.drawLine(x1,    y1,    x2,    y2,    color);
        gui_.drawLine(x1+px, y1+py, x2+px, y2+py, color);
    }
}

// ── 绘制：大号秒数（≤ 60s）──────────────────────────────────────

void Pomodoro::drawTimeLarge(uint32_t remSec) {
    char sBuf[3];
    snprintf(sBuf, sizeof(sBuf), "%02d", (int)remSec);

    // "SS" (2 chars) + 8px gap + "sec" (20×20)
    int totalH = kOsH + 8 + kSmH;
    int ty = 150 - totalH / 2;
    int tx = 200 - kOsW;   // 2 chars × kOsW centered

    gui_.setFont(&kFont_ascii_Oswald_Light_28_40);
    gui_.fillRect(tx, ty, 2 * kOsW, kOsH, ColorWhite);
    gui_.drawText(tx, ty, sBuf, ColorBlack, ColorWhite);

    // "sec" 标签
    int secX = 200 - (3 * kSmW) / 2;
    int secY = ty + kOsH + 8;
    gui_.setFont(&kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20);
    gui_.fillRect(secX, secY, 3 * kSmW, kSmH, ColorWhite);
    gui_.drawText(secX, secY, "sec", ColorBlack, ColorWhite);
}

// ── 绘制：小号时间（> 60s，MM:SS 或 HH:MM）──────────────────────

void Pomodoro::drawTimeSmall(const char *fmtStr) {
    // Oswald 28×40，5 字符宽 140px，水平居中
    int x = 200 - (5 * kOsW) / 2;   // = 130
    int y = 150 - kOsH / 2;          // = 130
    gui_.setFont(&kFont_ascii_Oswald_Light_28_40);
    gui_.fillRect(x, y, 5 * kOsW, kOsH, ColorWhite);
    gui_.drawText(x, y, fmtStr, ColorBlack, ColorWhite);
}
