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

    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), isrKey1, FALLING);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), isrKey2, FALLING);

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

// ── ISR（FALLING 边沿，最小化工作量）────────────────────────

void IRAM_ATTR InputKeyManager::isrKey1() { onGpioFalling(0); }
void IRAM_ATTR InputKeyManager::isrKey2() { onGpioFalling(1); }

void IRAM_ATTR InputKeyManager::onGpioFalling(int idx) {
    if (!instance_) return;

    BaseType_t woken = pdFALSE;

    if (instance_->activeKey_ == -1) {
        // 无活跃按键：认领并启动去抖定时器
        instance_->activeKey_ = idx;
        instance_->phase_     = Phase::DEBOUNCE;
        xTimerChangePeriodFromISR(instance_->timer_, pdMS_TO_TICKS(DEBOUNCE_MS), &woken);
        // xTimerChangePeriodFromISR 会自动启动定时器
    } else if (instance_->activeKey_ == idx &&
               instance_->phase_ == Phase::DEBOUNCE) {
        // 同一按键在去抖期间再次抖动：重置定时器延长等待
        xTimerResetFromISR(instance_->timer_, &woken);
    }
    // 其他键按下、或活跃键已进入 LONG_PRESS/REPEAT：忽略

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
            // 长按前释放：短按，派发 UP
            activeKey_ = -1;
            fireEvent(kIds[idx], KeyAction::UP);
        }
        break;

    case Phase::REPEAT:
        if (held) {
            // 仍在按下：派发重复事件，重启定时器
            fireEvent(kIds[idx], KeyAction::LONG_REPEAT);
            xTimerStart(timer_, 0);
        } else {
            // 释放：派发 UP，释放活跃槽
            activeKey_ = -1;
            fireEvent(kIds[idx], KeyAction::UP);
        }
        break;
    }
}
