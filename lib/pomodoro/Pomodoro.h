#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <app_message.h>

// ── 番茄时钟状态机 ─────────────────────────────────────────────
// 仿 Android CountDownTimer：内部驱动 onTick / onFinished，
// 所有业务更新通过 MSG_POMODORO_UPDATE 投递到消息队列，由 main loop 统一渲染。
//
// 按键事件由外部 PomodoroState 通过 onKey1() / onKey2Short() 注入，
// 不再内嵌 GPIO 轮询逻辑。

class Pomodoro {
public:
    // 在 setup() 中调用
    //   queue          — 消息队列句柄
    //   defaultSec     — 设置界面初始时长（秒），默认 25 分钟
    //   tickIntervalMs — onTick 调用间隔（毫秒），默认 1000ms
    void begin(QueueHandle_t queue,
               uint32_t defaultSec     = 25 * 60,
               uint32_t tickIntervalMs = 1000);

    // 在每次 loop() 中调用：驱动计时器和闪烁（不含按键逻辑）
    void update();

    // 是否处于番茄时钟模式（true 时主界面不刷新）
    bool isActive() const { return state_ != State::IDLE; }

    // ── 按键事件入口（由 PomodoroState::onKeyEvent 调用）──────
    void onKey1();      // 切换焦点 / 退出倒计时
    void onKey2Short(); // 增量 / 确认 / 退出

    // 直接进入设置界面（PomodoroState::onEnter 调用）
    void enterSetup();

    // 强制退出：回 IDLE 并投递 EXIT 消息（PomodoroState::onExit 调用）
    void reset();

private:
    enum class State : uint8_t { IDLE, SETUP, COUNTDOWN, FINISHED };
    enum class Focus : uint8_t { HOURS, MINUTES, EXIT, START };

    QueueHandle_t queue_          = nullptr;
    uint32_t      tickIntervalMs_ = 1000;

    State   state_  = State::IDLE;
    Focus   focus_  = Focus::HOURS;
    uint8_t setH_   = 25;
    uint8_t setM_   = 0;

    uint32_t totalSec_   = 0;
    uint32_t remSec_     = 0;
    uint32_t lastTickMs_ = 0;

    uint32_t lastBlinkMs_ = 0;
    bool     blinkOn_     = false;

    void enterCountdown();
    void enterIdle();

    void onTick(uint32_t remSec);
    void onFinished();

    void postMsg(AppMessage& msg);
    void postSetup();
};
