#include "PomodoroState.h"
#include <StateManager.h>
#include <ui_pomodoro.h>

PomodoroState::PomodoroState(Gui& gui, Pomodoro& pomodoro)
    : gui_(gui), pomodoro_(pomodoro) {}

// ── 状态生命周期 ──────────────────────────────────────────────

void PomodoroState::onEnter() {
    // 直接进入设置界面，无需再等待 KEY1 触发
    pomodoro_.enterSetup();
}

void PomodoroState::onExit() {
    // 若用户从外部切走（例如 StateManager 强制跳转），确保 Pomodoro 回到 IDLE
    pomodoro_.reset();
}

// ── 消息处理 ──────────────────────────────────────────────────

void PomodoroState::onMessage(const AppMessage& msg) {
    if (msg.type != MSG_POMODORO_UPDATE) return;

    if (msg.pomodoro.event == PomodoroEvent::EXIT) {
        // Pomodoro 内部发出退出信号 → 切换到下一个状态
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
