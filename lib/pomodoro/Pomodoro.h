#pragma once

#include <Arduino.h>

class Gui;

// ── 番茄时钟状态机 ─────────────────────────────────────────────
// KEY1 (GPIO18) : 循环切换焦点 HOURS→MINUTES→EXIT→START→HOURS
// KEY2 (GPIO0)  : HOURS/MINUTES 短按+1（99→0循环），长按快速累加
//                 EXIT 焦点下按下 = 退出；START 焦点下按下 = 开始倒计时
// KEY3 (EN引脚) : 硬件复位引脚，无法映射为软件 GPIO，无法在软件中使用

class Pomodoro {
public:
    static constexpr int      KEY1_PIN      = 18;
    static constexpr int      KEY2_PIN      = 0;
    static constexpr uint32_t LONG_PRESS_MS = 500;

    explicit Pomodoro(Gui &gui);

    // 在 setup() 中调用，初始化按键 GPIO
    void begin();

    // 在每次 loop() 开头调用：轮询按键、更新倒计时、绘制 UI
    void update();

    // 是否处于番茄时钟模式（true 时主界面不刷新）
    bool isActive() const { return state_ != State::IDLE; }

private:
    enum class State : uint8_t { IDLE, SETUP, COUNTDOWN, FINISHED };

    // 设置界面焦点：分 → 秒 → [EXIT] / [START]（KEY2 在按钮行横向切换）
    enum class Focus : uint8_t { HOURS, MINUTES, EXIT, START };

    Gui    &gui_;
    State   state_  = State::IDLE;
    Focus   focus_  = Focus::HOURS;
    uint8_t setH_   = 0;    // 设置：分（0-99）
    uint8_t setM_   = 25;   // 设置：秒（0-99，默认 25）

    uint32_t totalSec_      = 0;
    uint32_t remSec_        = 0;
    uint32_t lastTickMs_    = 0;

    uint32_t lastBlinkMs_   = 0;
    bool     blinkOn_       = false;

    // 按键状态
    bool     key1Prev_      = false;
    bool     key2Prev_      = false;
    uint32_t key2PressMs_   = 0;
    bool     key2LongFired_ = false;
    uint32_t key2RepeatMs_  = 0;   // 长按快速累加节拍时间戳

    void pollButtons();
    void onKey1();
    void onKey2Short();   // 短按 / 长按快速累加复用同一动作

    void enterSetup();
    void enterCountdown();
    void enterFinished();
    void enterIdle();

    void drawSetup();
    void drawCountdown();
    void drawFinished();
    void drawProgressRing(uint32_t remSec, uint32_t totalSec);
    void drawTimeLarge(uint32_t remSec);      // ≤ 60s：大号 SS + "sec"
    void drawTimeSmall(const char *fmtStr);   // > 60s：小号 MM:SS 或 HH:MM
};
