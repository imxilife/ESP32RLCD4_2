#include <Arduino.h>
#include <Wire.h>
#include <display_bsp.h>
#include <Gui.h>
#include <RTC85063.h>
#include <Humiture.h>
#include <GuiTests.h>
#include <WiFiConfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <app_message.h>
#include <Pomodoro.h>

#include <InputKeyManager.h>
#include <StateManager.h>
#include <MainUIState.h>
#include <PomodoroState.h>
#include <MusicPlayerState.h>
#include <XZAIState.h>

#define ENABLE_GUI_TESTS 0

// ── 硬件实例（全局，跨模块共用）──────────────────────────────

DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);
Gui         gui(&RlcdPort, 400, 300);

RTC85063   rtc;
Humiture   humiture;
WiFiConfig wifiConfig;
Pomodoro   pomodoro;

QueueHandle_t g_msgQueue = nullptr;

// ── 状态机与按键管理 ──────────────────────────────────────────

InputKeyManager   keyManager;
StateManager      stateManager;

MainUIState       stateMainUI(gui, rtc);
PomodoroState     statePomodoro(gui, pomodoro);
MusicPlayerState  stateMusic(gui);
XZAIState         stateXzai(gui);

// ── 预留中断引脚（触摸屏，待硬件确认后启用）──────────────────

static constexpr int kTouchIntPin = -1;

void IRAM_ATTR onTouchISR() {
    if (g_msgQueue == nullptr) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type    = MSG_TOUCH_EVENT;
    msg.touch.x = 0;
    msg.touch.y = 0;
    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

// ── setup ─────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // I2C 总线（RTC PCF85063 + 温湿度 SHTC3 共用）
    Wire.begin(13, 14);

    // LCD 初始化
    RlcdPort.RLCD_Init();
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);
    gui.clear();
    gui.display();

    // 消息队列
    g_msgQueue = xQueueCreate(16, sizeof(AppMessage));
    if (g_msgQueue == nullptr) {
        Serial.println("创建消息队列失败！");
        while (1) delay(100);
    }

    // WiFi / NTP 回调：注册到 MainUIState 的静态方法
    stateMainUI.registerCallbacks(wifiConfig);

    // 番茄时钟初始化（绑定队列，不含 GPIO 操作）
    pomodoro.begin(g_msgQueue, 25 * 60, 1000);

    // 电池 ADC
    analogSetPinAttenuation(4, ADC_11db);

    // 按键管理器：GPIO 中断 + FreeRTOS 定时器去抖
    keyManager.begin();
    keyManager.addListener([](const KeyEvent& e) {
        stateManager.dispatchKeyEvent(e);
    });

    // 注册并启动状态机（后台任务在 MainUIState::onEnter 首次调用时创建）
    stateManager.registerState(StateId::MAIN_UI,      &stateMainUI);
    stateManager.registerState(StateId::POMODORO,     &statePomodoro);
    stateManager.registerState(StateId::MUSIC_PLAYER, &stateMusic);
    stateManager.registerState(StateId::XZAI,         &stateXzai);
    stateManager.begin(StateId::MAIN_UI);

#if ENABLE_GUI_TESTS
    GuiTests::runAllTests(gui);
    delay(5000);
    gui.clear();
    gui.display();

        // 触摸中断（有引脚时启用）
    if (kTouchIntPin >= 0) {
        pinMode(kTouchIntPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kTouchIntPin), onTouchISR, FALLING);
    }
#endif
}

// ── loop（UI Looper）─────────────────────────────────────────

void loop() {
    if (g_msgQueue == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    // 番茄时钟计时驱动（tick / blink，不含按键逻辑）
    pomodoro.update();

    // 从队列取消息，转发给当前激活状态
    AppMessage msg;
    if (xQueueReceive(g_msgQueue, &msg, pdMS_TO_TICKS(50)) == pdTRUE) {
        stateManager.dispatch(msg);
    }

    // 统一在主线程刷屏
    gui.display();
}
