#include "InputKeyManager.h"
#include <Arduino.h>

InputKeyManager* InputKeyManager::instance_ = nullptr;

const int   InputKeyManager::kPins[2] = { KEY1_PIN, KEY2_PIN };
const KeyId InputKeyManager::kIds[2]  = { KeyId::KEY1, KeyId::KEY2 };

// ── 初始化 ────────────────────────────────────────────────────

void InputKeyManager::begin() {
    instance_  = this;
    activeKey_ = -1;

    for (int i = 0; i < 2; i++) {
        pinMode(kPins[i], INPUT_PULLUP);
    }

    // 一个 one-shot 定时器，周期在各阶段动态调整
    timer_ = xTimerCreate("keyTimer",
                          pdMS_TO_TICKS(DEBOUNCE_MS),
                          pdFALSE,   // one-shot
                          nullptr,
                          timerCb);

    // CHANGE 模式同时捕获 FALLING（按下）和 RISING（释放），
    // 使按键释放能被立即感知，消除 LONG_PRESS 窗口期内丢失按键的问题
    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), isrKey1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), isrKey2, CHANGE);

    Serial.printf("[InputKey] begin: KEY1=GPIO%d KEY2=GPIO%d\n", KEY1_PIN, KEY2_PIN);
}

void InputKeyManager::addListener(KeyListener listener) {
    listeners_.push_back(std::move(listener));
}

// ── 事件派发 ──────────────────────────────────────────────────

void InputKeyManager::fireEvent(KeyId id, KeyAction action) {
    KeyEvent e{ id, action };
    for (auto& l : listeners_) l(e);

#if CORE_DEBUG_LEVEL >= 4
    Serial.printf("[InputKey] KEY%d %s\n",
                  (int)id + 1,
                  action == KeyAction::DOWN        ? "DOWN"        :
                  action == KeyAction::UP          ? "UP"          :
                  action == KeyAction::LONG_PRESS  ? "LONG_PRESS"  : "LONG_REPEAT");
#endif
}

// ── ISR（CHANGE 模式，同时响应 FALLING 和 RISING）────────────

void IRAM_ATTR InputKeyManager::isrKey1() { onGpioChange(0); }
void IRAM_ATTR InputKeyManager::isrKey2() { onGpioChange(1); }

void IRAM_ATTR InputKeyManager::onGpioChange(int idx) {
    if (!instance_) return;

    BaseType_t woken = pdFALSE;
    bool pressed = (digitalRead(instance_->kPins[idx]) == LOW);

    if (pressed) {
        // ── FALLING：按键按下 ──────────────────────────────────
        if (instance_->activeKey_ == -1) {
            // 无活跃按键：认领并启动去抖定时器
            instance_->activeKey_ = idx;
            instance_->phase_     = Phase::DEBOUNCE;
            xTimerChangePeriodFromISR(instance_->timer_, pdMS_TO_TICKS(DEBOUNCE_MS), &woken);
        } else if (instance_->activeKey_ == idx && instance_->phase_ == Phase::DEBOUNCE) {
            // 同一按键在去抖期间再次抖动：重置定时器
            xTimerResetFromISR(instance_->timer_, &woken);
        }
        // LONG_PRESS / REPEAT / UP_PENDING：活跃键未释放或正在收尾，忽略
    } else {
        // ── RISING：按键释放 ───────────────────────────────────
        if (instance_->activeKey_ == idx) {
            Phase p = instance_->phase_;
            if (p == Phase::LONG_PRESS || p == Phase::REPEAT) {
                // 立即调度 UP：切换到 UP_PENDING，用 1ms 定时器在 timer task 派发
                // （fireEvent 不能在 ISR 直接调用，其监听者可能执行非 ISR 安全操作）
                instance_->phase_ = Phase::UP_PENDING;
                xTimerChangePeriodFromISR(instance_->timer_, pdMS_TO_TICKS(1), &woken);
            }
            // DEBOUNCE：抖动期内释放，由定时器检查 GPIO 高低判断是否为噪声
            // UP_PENDING：已在等待派发，忽略
        }
    }

    if (woken == pdTRUE) portYIELD_FROM_ISR();
}

// ── 定时器回调（timer daemon task 上下文）────────────────────

void InputKeyManager::timerCb(TimerHandle_t t) {
    if (instance_) instance_->onTimer();
}

void InputKeyManager::onTimer() {
    int idx = activeKey_;
    if (idx < 0) return;

    bool held = (digitalRead(kPins[idx]) == LOW);

    switch (phase_) {
    case Phase::DEBOUNCE:
        if (held) {
            // 去抖确认按下：派发 DOWN，进入长按检测阶段
            phase_ = Phase::LONG_PRESS;
            fireEvent(kIds[idx], KeyAction::DOWN);
            xTimerChangePeriod(timer_, pdMS_TO_TICKS(LONG_PRESS_MS), 0);
        } else {
            // 去抖期间已释放：视为噪声
            activeKey_ = -1;
        }
        break;

    case Phase::LONG_PRESS:
        if (held) {
            // 持续按下超过 LONG_PRESS_MS：进入重复阶段
            phase_ = Phase::REPEAT;
            fireEvent(kIds[idx], KeyAction::LONG_PRESS);
            xTimerChangePeriod(timer_, pdMS_TO_TICKS(REPEAT_MS), 0);
        } else {
            // 长按前释放（RISING ISR 兜底）：短按，派发 UP
            activeKey_ = -1;
            fireEvent(kIds[idx], KeyAction::UP);
        }
        break;

    case Phase::UP_PENDING:
        // RISING ISR 检测到释放后调度的 1ms 定时器到期：派发 UP
        activeKey_ = -1;
        fireEvent(kIds[idx], KeyAction::UP);
        break;

    case Phase::REPEAT:
        if (held) {
            // 仍在按下：派发重复事件，重启定时器
            fireEvent(kIds[idx], KeyAction::LONG_REPEAT);
            xTimerStart(timer_, 0);
        } else {
            // 释放（RISING ISR 兜底）：派发 UP，释放活跃槽
            activeKey_ = -1;
            fireEvent(kIds[idx], KeyAction::UP);
        }
        break;
    }
}
