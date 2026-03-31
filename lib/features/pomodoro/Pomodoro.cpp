#include "Pomodoro.h"

// ── 初始化 ────────────────────────────────────────────────────

void Pomodoro::begin(QueueHandle_t queue, uint32_t defaultSec, uint32_t tickIntervalMs) {
    queue_          = queue;
    tickIntervalMs_ = tickIntervalMs;
    setH_           = (uint8_t)(defaultSec / 60);
    setM_           = (uint8_t)(defaultSec % 60);
    Serial.printf("[Pomodoro] begin: default=%us tick=%ums\n", defaultSec, tickIntervalMs);

    if (tickTimer_ == nullptr) {
        tickTimer_ = xTimerCreate("pomTick", pdMS_TO_TICKS(100),
                                  pdTRUE, this, timerCallback);
    }
}

void Pomodoro::timerCallback(TimerHandle_t xTimer) {
    static_cast<Pomodoro*>(pvTimerGetTimerID(xTimer))->update();
}

void Pomodoro::startTick() { if (tickTimer_) xTimerStart(tickTimer_, 0); }
void Pomodoro::stopTick()  { if (tickTimer_) xTimerStop(tickTimer_, 0);  }

// ── 状态生命周期 ──────────────────────────────────────────────

void Pomodoro::enterSetup() {
    phase_  = Phase::SETUP;
    focus_  = Focus::HOURS;
    postSetup();
}

void Pomodoro::reset() {
    // 静默回到 SETUP，不发消息（由 PomodoroState::onExit 调用做清理）
    phase_    = Phase::SETUP;
    finished_ = false;
}

// ── 主循环（timerCallback 每 100ms 调用）─────────────────────

void Pomodoro::update() {
    if (phase_ != Phase::COUNTDOWN) return;

    uint32_t now = millis();

    if (!finished_) {
        // 倒计时阶段
        if (now - lastTickMs_ >= tickIntervalMs_) {
            lastTickMs_ += tickIntervalMs_;
            if (remSec_ > 0) {
                remSec_--;
                onTick(remSec_);
            } else {
                onFinished();
            }
        }
    } else {
        // 闪烁阶段
        if (now - lastBlinkMs_ >= 500) {
            lastBlinkMs_ = now;
            blinkOn_ = !blinkOn_;

            AppMessage msg;
            msg.type                      = MSG_POMODORO_UPDATE;
            msg.pomodoro.event            = PomodoroEvent::FINISHED;
            msg.pomodoro.finished.blinkOn = blinkOn_;
            postMsg(msg);
        }
    }
}

// ── 倒计时回调 ────────────────────────────────────────────────

void Pomodoro::onTick(uint32_t remSec) {
    AppMessage msg;
    msg.type                   = MSG_POMODORO_UPDATE;
    msg.pomodoro.event         = PomodoroEvent::TICK;
    msg.pomodoro.tick.remSec   = remSec;
    msg.pomodoro.tick.totalSec = totalSec_;
    postMsg(msg);
}

void Pomodoro::onFinished() {
    finished_    = true;
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
    switch (phase_) {
    case Phase::SETUP:
        // 循环切换焦点：HOURS → MINUTES → EXIT → START → HOURS
        switch (focus_) {
        case Focus::HOURS:   focus_ = Focus::MINUTES; break;
        case Focus::MINUTES: focus_ = Focus::EXIT;    break;
        case Focus::EXIT:    focus_ = Focus::START;   break;
        case Focus::START:   focus_ = Focus::HOURS;   break;
        }
        postSetup();
        break;

    case Phase::COUNTDOWN:
        // 倒计时阶段忽略短按，退出交给 PomodoroState 的 KEY1 长按处理
        break;
    }
}

void Pomodoro::onKey2Short() {
    if (phase_ != Phase::SETUP) return;

    switch (focus_) {
    case Focus::HOURS:
        setH_ = (setH_ + 1) % 100;
        postSetup();
        break;
    case Focus::MINUTES:
        setM_ = (setM_ + 1) % 60;
        postSetup();
        break;
    case Focus::EXIT:
        postExit();
        break;
    case Focus::START:
        enterCountdown();
        break;
    }
}

// ── 状态切换（私有）──────────────────────────────────────────

void Pomodoro::enterCountdown() {
    uint32_t total = (uint32_t)setH_ * 60u + (uint32_t)setM_;
    if (total == 0) {
        // 时长为 0，拒绝开始，焦点移到"秒"字段提示用户
        focus_ = Focus::MINUTES;
        postSetup();
        return;
    }
    phase_      = Phase::COUNTDOWN;
    finished_   = false;
    totalSec_   = total;
    remSec_     = total;
    lastTickMs_ = millis();
    onTick(remSec_);  // 立即刷新倒计时界面
}

void Pomodoro::postExit() {
    AppMessage msg;
    msg.type           = MSG_POMODORO_UPDATE;
    msg.pomodoro.event = PomodoroEvent::EXIT;
    postMsg(msg);
}

// ── 辅助 ─────────────────────────────────────────────────────

void Pomodoro::postSetup() {
    AppMessage msg;
    msg.type                   = MSG_POMODORO_UPDATE;
    msg.pomodoro.event         = PomodoroEvent::SETUP;
    msg.pomodoro.setup.hours   = setH_;
    msg.pomodoro.setup.minutes = setM_;
    msg.pomodoro.setup.focus   = (uint8_t)focus_;
    postMsg(msg);
}

void Pomodoro::postMsg(AppMessage& msg) {
    if (queue_) xQueueSend(queue_, &msg, 0);
}
