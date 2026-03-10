#include "InputKeyManager.h"
#include <Arduino.h>

InputKeyManager* InputKeyManager::instance_ = nullptr;

// ── 初始化 ────────────────────────────────────────────────────

void InputKeyManager::begin() {
    instance_ = this;

    const int   pins[2] = { KEY1_PIN, KEY2_PIN };
    const KeyId ids[2]  = { KeyId::KEY1, KeyId::KEY2 };

    for (int i = 0; i < 2; i++) {
        KeyData& k = keys_[i];
        k.pin   = pins[i];
        k.id    = ids[i];
        k.state = KeyState::IDLE;

        // pvTimerID 编码为 key 索引，供定时器回调识别
        k.debounceTimer  = xTimerCreate("kDebounce",
                                        pdMS_TO_TICKS(DEBOUNCE_MS),
                                        pdFALSE,
                                        (void*)(uintptr_t)i,
                                        debounceTimerCb);
        k.longPressTimer = xTimerCreate("kLongPress",
                                        pdMS_TO_TICKS(LONG_PRESS_MS),
                                        pdFALSE,
                                        (void*)(uintptr_t)i,
                                        longPressTimerCb);
        // repeat 定时器使用自动重载（pdTRUE），在回调内判断是否继续
        k.repeatTimer    = xTimerCreate("kRepeat",
                                        pdMS_TO_TICKS(REPEAT_MS),
                                        pdTRUE,
                                        (void*)(uintptr_t)i,
                                        repeatTimerCb);

        pinMode(k.pin, INPUT_PULLUP);
    }

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
    KeyData& k = instance_->keys_[idx];

    BaseType_t woken = pdFALSE;

    switch (k.state) {
    case KeyState::IDLE:
        k.state = KeyState::DEBOUNCING;
        xTimerStartFromISR(k.debounceTimer, &woken);
        break;
    case KeyState::DEBOUNCING:
        // 再次抖动：重置去抖定时器延长等待
        xTimerResetFromISR(k.debounceTimer, &woken);
        break;
    case KeyState::PRESSED:
    case KeyState::LONG_PRESSING:
        // 已按下状态下的 FALLING 噪声，忽略
        break;
    }

    if (woken == pdTRUE) portYIELD_FROM_ISR();
}

// ── 定时器回调（运行于 FreeRTOS timer daemon task）────────────

void InputKeyManager::debounceTimerCb(TimerHandle_t t) {
    int idx = (int)(uintptr_t)pvTimerGetTimerID(t);
    if (instance_) instance_->onDebounceTimer(idx);
}

void InputKeyManager::longPressTimerCb(TimerHandle_t t) {
    int idx = (int)(uintptr_t)pvTimerGetTimerID(t);
    if (instance_) instance_->onLongPressTimer(idx);
}

void InputKeyManager::repeatTimerCb(TimerHandle_t t) {
    int idx = (int)(uintptr_t)pvTimerGetTimerID(t);
    if (instance_) instance_->onRepeatTimer(idx);
}

void InputKeyManager::onDebounceTimer(int idx) {
    KeyData& k = keys_[idx];
    if (digitalRead(k.pin) == LOW) {
        // 去抖确认：按键确实已按下
        k.state = KeyState::PRESSED;
        fireEvent(k.id, KeyAction::DOWN);
        xTimerStart(k.longPressTimer, 0);
    } else {
        // 去抖期间已释放：视为噪声
        k.state = KeyState::IDLE;
    }
}

void InputKeyManager::onLongPressTimer(int idx) {
    KeyData& k = keys_[idx];
    if (digitalRead(k.pin) == LOW) {
        // 持续按下超过 LONG_PRESS_MS：进入长按状态
        k.state = KeyState::LONG_PRESSING;
        fireEvent(k.id, KeyAction::LONG_PRESS);
        xTimerStart(k.repeatTimer, 0);
    } else {
        // 在长按定时器到期前已释放：短按，派发 UP
        k.state = KeyState::IDLE;
        fireEvent(k.id, KeyAction::UP);
    }
}

void InputKeyManager::onRepeatTimer(int idx) {
    KeyData& k = keys_[idx];
    if (digitalRead(k.pin) == LOW) {
        // 仍在按下：派发重复事件（定时器自动重载，继续触发）
        fireEvent(k.id, KeyAction::LONG_REPEAT);
    } else {
        // 已释放：停止重复定时器，派发 UP
        xTimerStop(k.repeatTimer, 0);
        k.state = KeyState::IDLE;
        fireEvent(k.id, KeyAction::UP);
    }
}
