#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <core/app_message/app_message.h>

// ── 番茄时钟状态机 ─────────────────────────────────────────────
// 内部两个阶段：
//   SETUP    — 设置时长（分/秒）、EXIT/START 按键
//   COUNTDOWN — 倒计时；remSec_==0 后进入 finished_ 闪烁子阶段
//
// 按键事件由外部 PomodoroState 通过 onKey1() / onKey2Short() 注入。
// 所有 UI 更新通过 MSG_POMODORO_UPDATE 投递到消息队列，由 main loop 统一渲染。

class Pomodoro {
public:
    // 在首次进入 PomodoroState 时调用（懒初始化）
    //   queue          — 消息队列句柄
    //   defaultSec     — 设置界面初始时长（秒），默认 25 分钟
    //   tickIntervalMs — 倒计时 tick 间隔（毫秒），默认 1000ms
    void begin(QueueHandle_t queue,
               uint32_t defaultSec     = 25 * 60,
               uint32_t tickIntervalMs = 1000);

    // 启动 / 停止内部节拍定时器（由 PomodoroState::onEnter/onExit 调用）
    void startTick();
    void stopTick();

    // 进入设置界面（PomodoroState::onEnter 调用）
    void enterSetup();

    // 静默重置回 SETUP（PomodoroState::onExit 调用，不发消息）
    void reset();

    // ── 按键事件入口（由 PomodoroState::onKeyEvent 调用）──────
    void onKey1();      // SETUP: 切换焦点；COUNTDOWN: 忽略短按
    void onKey2Short(); // SETUP: 执行焦点动作（增量/确认/退出/开始）

private:
    enum class Phase : uint8_t { SETUP, COUNTDOWN };
    enum class Focus : uint8_t { HOURS, MINUTES, EXIT, START };

    QueueHandle_t queue_          = nullptr;
    uint32_t      tickIntervalMs_ = 1000;
    TimerHandle_t tickTimer_      = nullptr;

    Phase   phase_    = Phase::SETUP;
    bool    finished_ = false;       // true: 倒计时结束，进入闪烁子阶段

    Focus   focus_  = Focus::HOURS;
    uint8_t setH_   = 25;            // 设置界面"分"字段
    uint8_t setM_   = 0;             // 设置界面"秒"字段

    uint32_t totalSec_   = 0;
    uint32_t remSec_     = 0;
    uint32_t lastTickMs_ = 0;

    uint32_t lastBlinkMs_ = 0;
    bool     blinkOn_     = false;

    // ── 内部方法 ──────────────────────────────────────────────
    void enterCountdown();

    void onTick(uint32_t remSec);    // 每秒触发：发送 TICK 消息
    void onFinished();               // remSec_==0 时触发：切入 finished_ 模式

    void postExit();                 // 发送 EXIT 消息（替代原 enterIdle）
    void postSetup();                // 发送 SETUP 消息（刷新设置界面）
    void postMsg(AppMessage& msg);   // 向队列投递消息

    // FreeRTOS 软件定时器回调
    static void timerCallback(TimerHandle_t xTimer);

    // update() 由 timerCallback 每 100ms 调用
    void update();
};
