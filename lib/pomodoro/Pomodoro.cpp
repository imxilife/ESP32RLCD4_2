#include "Pomodoro.h"

// ── 初始化 ────────────────────────────────────────────────────

void Pomodoro::begin(QueueHandle_t queue, uint32_t defaultSec, uint32_t tickIntervalMs) {
    queue_          = queue;
    tickIntervalMs_ = tickIntervalMs;
    setH_           = (uint8_t)(defaultSec / 60);
    setM_           = (uint8_t)(defaultSec % 60);
    Serial.printf("[Pomodoro] begin: default=%us tick=%ums\n", defaultSec, tickIntervalMs);
}

// ── 主循环入口 ────────────────────────────────────────────────

void Pomodoro::update() {
    switch (state_) {
    case State::IDLE:
    case State::SETUP:
        break;

    case State::COUNTDOWN: {
        uint32_t now = millis();
        if (now - lastTickMs_ >= tickIntervalMs_) {
            lastTickMs_ += tickIntervalMs_;
            if (remSec_ > 0) {
                remSec_--;
                onTick(remSec_);
            } else {
                onFinished();
            }
        }
        break;
    }

    case State::FINISHED: {
        uint32_t now = millis();
        if (now - lastBlinkMs_ >= 500) {
            lastBlinkMs_ = now;
            blinkOn_ = !blinkOn_;

            AppMessage msg;
            msg.type                   = MSG_POMODORO_UPDATE;
            msg.pomodoro.event         = PomodoroEvent::FINISHED;
            msg.pomodoro.finished.blinkOn = blinkOn_;
            postMsg(msg);
        }
        break;
    }
    }
}

// ── CountDownTimer 回调 ───────────────────────────────────────

void Pomodoro::onTick(uint32_t remSec) {
    AppMessage msg;
    msg.type                   = MSG_POMODORO_UPDATE;
    msg.pomodoro.event         = PomodoroEvent::TICK;
    msg.pomodoro.tick.remSec   = remSec;
    msg.pomodoro.tick.totalSec = totalSec_;
    postMsg(msg);
}

void Pomodoro::onFinished() {
    state_       = State::FINISHED;
    blinkOn_     = true;
    lastBlinkMs_ = millis();

    AppMessage msg;
    msg.type                      = MSG_POMODORO_UPDATE;
    msg.pomodoro.event            = PomodoroEvent::FINISHED;
    msg.pomodoro.finished.blinkOn = blinkOn_;
    postMsg(msg);
}

// ── 按键事件（由 PomodoroState::onKeyEvent 注入）────────────────

void Pomodoro::onKey1() {
    switch (state_) {
    case State::IDLE:
        enterSetup();
        break;
    case State::SETUP:
        switch (focus_) {
        case Focus::HOURS:   focus_ = Focus::MINUTES; break;
        case Focus::MINUTES: focus_ = Focus::EXIT;    break;
        case Focus::EXIT:    focus_ = Focus::START;   break;
        case Focus::START:   focus_ = Focus::HOURS;   break;
        }
        postSetup();
        break;
    case State::COUNTDOWN:
    case State::FINISHED:
        enterIdle();
        break;
    }
}

void Pomodoro::onKey2Short() {
    if (state_ != State::SETUP) return;
    switch (focus_) {
    case Focus::HOURS:
        setH_ = (setH_ + 1) % 100;
        postSetup();
        break;
    case Focus::MINUTES:
        setM_ = (setM_ + 1) % 100;
        postSetup();
        break;
    case Focus::EXIT:
        enterIdle();
        break;
    case Focus::START:
        enterCountdown();
        break;
    }
}

// ── 状态切换 ─────────────────────────────────────────────────

void Pomodoro::enterSetup() {
    state_ = State::SETUP;
    focus_ = Focus::HOURS;
    postSetup();
}

void Pomodoro::enterCountdown() {
    uint32_t t = (uint32_t)setH_ * 60u + (uint32_t)setM_;
    if (t == 0) {
        focus_ = Focus::MINUTES;
        postSetup();
        return;
    }
    totalSec_   = t;
    remSec_     = t;
    lastTickMs_ = millis();
    state_      = State::COUNTDOWN;
    onTick(remSec_);  // 立即发送初始画面
}

void Pomodoro::enterIdle() {
    state_ = State::IDLE;
    AppMessage msg;
    msg.type           = MSG_POMODORO_UPDATE;
    msg.pomodoro.event = PomodoroEvent::EXIT;
    postMsg(msg);
}

void Pomodoro::reset() {
    if (state_ != State::IDLE) {
        enterIdle();
    }
}

// ── 辅助 ─────────────────────────────────────────────────────

void Pomodoro::postSetup() {
    AppMessage msg;
    msg.type                    = MSG_POMODORO_UPDATE;
    msg.pomodoro.event          = PomodoroEvent::SETUP;
    msg.pomodoro.setup.hours    = setH_;
    msg.pomodoro.setup.minutes  = setM_;
    msg.pomodoro.setup.focus    = (uint8_t)focus_;
    postMsg(msg);
}

void Pomodoro::postMsg(AppMessage& msg) {
    if (queue_) xQueueSend(queue_, &msg, 0);
}
