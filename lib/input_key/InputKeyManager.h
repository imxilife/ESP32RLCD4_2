#pragma once

#include "KeyEvent.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <vector>

// 按键管理器：中断驱动 + 单共享 FreeRTOS 定时器
//
// 同一时刻只有一个按键处于活跃处理中（activeKey_），
// 单定时器按阶段调整周期，依次驱动：去抖 → 长按检测 → 重复
//
// 状态机（Phase）：
//   IDLE → [FALLING ISR]       → DEBOUNCE  (启动 200ms 定时器)
//   DEBOUNCE → [GPIO=LOW]      → LONG_PRESS (派发 DOWN，改期 500ms)
//   DEBOUNCE → [GPIO=HIGH]     → IDLE       (噪声，丢弃)
//   LONG_PRESS → [GPIO=LOW]    → REPEAT     (派发 LONG_PRESS，改期 120ms)
//   LONG_PRESS → [GPIO=HIGH]   → IDLE       (派发 UP，短按)
//   REPEAT → [GPIO=LOW]        → REPEAT     (派发 LONG_REPEAT，重启 120ms)
//   REPEAT → [GPIO=HIGH]       → IDLE       (派发 UP)

class InputKeyManager {
public:
    static constexpr int      KEY1_PIN      = 18;
    static constexpr int      KEY2_PIN      = 0;
    static constexpr uint32_t DEBOUNCE_MS   = 200;
    static constexpr uint32_t LONG_PRESS_MS = 500;
    static constexpr uint32_t REPEAT_MS     = 120;

    // 初始化：配置 GPIO、注册中断、创建共享定时器
    void begin();

    // 注册按键事件监听者（可多个）
    void addListener(KeyListener listener);

private:
    enum class Phase : uint8_t { DEBOUNCE, LONG_PRESS, REPEAT };

    static const int   kPins[2];
    static const KeyId kIds[2];

    TimerHandle_t            timer_     = nullptr;
    volatile int             activeKey_ = -1;   // -1 = 无活跃按键
    volatile Phase           phase_     = Phase::DEBOUNCE;

    std::vector<KeyListener> listeners_;

    static InputKeyManager* instance_;

    void fireEvent(KeyId id, KeyAction action);
    void onTimer();

    // ISR：FALLING 边沿触发
    static void IRAM_ATTR isrKey1();
    static void IRAM_ATTR isrKey2();
    static void IRAM_ATTR onGpioFalling(int idx);

    // FreeRTOS 定时器回调（在 timer daemon task 上下文执行）
    static void timerCb(TimerHandle_t t);
};
