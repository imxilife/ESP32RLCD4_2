#include "PomodoroState.h"
#include <StateManager.h>
#include <ui_pomodoro.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// g_msgQueue 由 main.cpp 定义，onEnter 首次初始化 Pomodoro 时使用
extern QueueHandle_t g_msgQueue;

PomodoroState::PomodoroState(Gui& gui)
    : gui_(gui) {}

// ── FreeRTOS 软件定时器回调 ───────────────────────────────────
// 运行在 FreeRTOS timer task 上下文（非 ISR），xQueueSend 安全可用。
// 每 100ms 调一次 update()，检查倒计时是否到期并更新闪烁状态；
// 内部按需向 g_msgQueue 发送 MSG_POMODORO_UPDATE，由 loop() 消费。

void PomodoroState::onTick(TimerHandle_t xTimer) {
    PomodoroState* self = static_cast<PomodoroState*>(pvTimerGetTimerID(xTimer));
    self->pomodoro_.update();
}

// ── 状态生命周期 ──────────────────────────────────────────────

void PomodoroState::onEnter() {
    // 首次进入时初始化 Pomodoro（once-flag 懒初始化）
    // 专注时长 25 分钟，tick 间隔 1 秒
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        pomodoro_.begin(g_msgQueue, 25 * 60, 1000);
    }

    // 启动节拍定时器（首次创建，此后复用）
    if (tickTimer_ == nullptr) {
        tickTimer_ = xTimerCreate("pomTick", pdMS_TO_TICKS(100),
                                  pdTRUE, this, onTick);
    }
    xTimerStart(tickTimer_, 0);

    // 直接进入设置界面，无需再等待 KEY1 触发
    pomodoro_.enterSetup();
}

void PomodoroState::onExit() {
    // 停止节拍定时器，避免在其他状态中继续触发
    if (tickTimer_ != nullptr) xTimerStop(tickTimer_, 0);

    // 若用户从外部切走（例如 StateManager 强制跳转），确保 Pomodoro 回到 IDLE
    pomodoro_.reset();
}

// ── 消息处理 ──────────────────────────────────────────────────

void PomodoroState::onMessage(const AppMessage& msg) {
    if (msg.type != MSG_POMODORO_UPDATE) return;

    if (msg.pomodoro.event == PomodoroEvent::EXIT) {
        requestTransition(StateId::MUSIC_PLAYER);
    } else {
        handlePomodoroUpdate(gui_, msg);
    }
}

// ── 按键事件（从 InputKeyManager 转发）────────────────────────

void PomodoroState::onKeyEvent(const KeyEvent& event) {
    switch (event.id) {
    case KeyId::KEY1:
        if (event.action == KeyAction::DOWN) {
            pomodoro_.onKey1();
        }
        break;
    case KeyId::KEY2:
        // DOWN / LONG_PRESS / LONG_REPEAT 均映射为 onKey2Short（与原逻辑一致）
        if (event.action == KeyAction::DOWN     ||
            event.action == KeyAction::LONG_PRESS ||
            event.action == KeyAction::LONG_REPEAT) {
            pomodoro_.onKey2Short();
        }
        break;
    }
}
