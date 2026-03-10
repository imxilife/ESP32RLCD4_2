#pragma once

#include "KeyEvent.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <vector>

// 按键管理器：中断驱动 + FreeRTOS 定时器去抖
//
// 每个按键内部状态机：
//   IDLE → [FALLING ISR] → DEBOUNCING (启动 200ms 定时器)
//   DEBOUNCING → [定时器, GPIO=LOW]  → PRESSED (派发 DOWN, 启动 500ms 长按定时器)
//   DEBOUNCING → [定时器, GPIO=HIGH] → IDLE (抖动噪声，忽略)
//   PRESSED → [长按定时器, GPIO=LOW]  → LONG_PRESSING (派发 LONG_PRESS, 启动 120ms 重复定时器)
//   PRESSED → [长按定时器, GPIO=HIGH] → IDLE (派发 UP，释放在长按前)
//   LONG_PRESSING → [重复定时器, GPIO=LOW]  → LONG_PRESSING (派发 LONG_REPEAT，定时器自动重载)
//   LONG_PRESSING → [重复定时器, GPIO=HIGH] → IDLE (派发 UP，停止定时器)

class InputKeyManager {
public:
    static constexpr int      KEY1_PIN      = 18;
    static constexpr int      KEY2_PIN      = 0;
    static constexpr uint32_t DEBOUNCE_MS   = 200;
    static constexpr uint32_t LONG_PRESS_MS = 500;
    static constexpr uint32_t REPEAT_MS     = 120;

    // 初始化：配置 GPIO、注册中断、创建 FreeRTOS 定时器
    void begin();

    // 注册按键事件监听者（可多个）
    void addListener(KeyListener listener);

private:
    enum class KeyState : uint8_t { IDLE, DEBOUNCING, PRESSED, LONG_PRESSING };

    struct KeyData {
        int               pin;
        KeyId             id;
        volatile KeyState state;
        TimerHandle_t     debounceTimer;
        TimerHandle_t     longPressTimer;
        TimerHandle_t     repeatTimer;
    };

    KeyData                  keys_[2];
    std::vector<KeyListener> listeners_;

    static InputKeyManager* instance_;

    void fireEvent(KeyId id, KeyAction action);

    void onDebounceTimer(int idx);
    void onLongPressTimer(int idx);
    void onRepeatTimer(int idx);

    // ISR：FALLING 边沿触发
    static void IRAM_ATTR isrKey1();
    static void IRAM_ATTR isrKey2();
    static void IRAM_ATTR onGpioFalling(int idx);

    // FreeRTOS 定时器回调（在 timer daemon task 上下文执行）
    static void debounceTimerCb(TimerHandle_t t);
    static void longPressTimerCb(TimerHandle_t t);
    static void repeatTimerCb(TimerHandle_t t);
};
