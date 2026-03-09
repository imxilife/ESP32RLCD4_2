#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <app_message.h>

// ── 番茄时钟状态机 ─────────────────────────────────────────────
// 仿 Android CountDownTimer：内部驱动 onTick / onFinished，
// 所有业务更新通过 MSG_POMODORO_UPDATE 投递到消息队列，由 main loop 统一渲染。
//
// KEY1 (GPIO18) : 循环切换焦点 HOURS→MINUTES→EXIT→START→HOURS
// KEY2 (GPIO0)  : HOURS/MINUTES 短按+1（99→0循环），长按快速累加
//                 EXIT 焦点下按下 = 退出；START 焦点下按下 = 开始倒计时

class Pomodoro {
public:
    static constexpr int      KEY1_PIN      = 18;
    static constexpr int      KEY2_PIN      = 0;
    static constexpr uint32_t LONG_PRESS_MS = 500;

    // 在 setup() 中调用
    //   queue          — 消息队列句柄
    //   defaultSec     — 设置界面初始时长（秒），默认 25 分钟
    //   tickIntervalMs — onTick 调用间隔（毫秒），默认 1000ms
    void begin(QueueHandle_t queue,
               uint32_t defaultSec     = 25 * 60,
               uint32_t tickIntervalMs = 1000);

    // 在每次 loop() 开头调用：轮询按键、驱动状态机
    void update();

    // 是否处于番茄时钟模式（true 时主界面不刷新）
    bool isActive() const { return state_ != State::IDLE; }

private:
    enum class State : uint8_t { IDLE, SETUP, COUNTDOWN, FINISHED };
    enum class Focus : uint8_t { HOURS, MINUTES, EXIT, START };

    QueueHandle_t queue_          = nullptr;
    uint32_t      tickIntervalMs_ = 1000;

    State   state_  = State::IDLE;
    Focus   focus_  = Focus::HOURS;
    uint8_t setH_   = 25;   // 设置：分钟（0-99，默认 25）
    uint8_t setM_   = 0;    // 设置：秒钟（0-99）

    uint32_t totalSec_      = 0;
    uint32_t remSec_        = 0;
    uint32_t lastTickMs_    = 0;

    uint32_t lastBlinkMs_   = 0;
    bool     blinkOn_       = false;

    bool     key1Prev_      = false;
    bool     key2Prev_      = false;
    uint32_t key2PressMs_   = 0;
    bool     key2LongFired_ = false;
    uint32_t key2RepeatMs_  = 0;

    void pollButtons();
    void onKey1();
    void onKey2Short();

    void enterSetup();
    void enterCountdown();
    void enterIdle();

    // CountDownTimer 风格回调：向队列投递消息
    void onTick(uint32_t remSec);
    void onFinished();

    // 辅助：构造并投递 MSG_POMODORO_UPDATE
    void postMsg(AppMessage& msg);
    void postSetup();
};
