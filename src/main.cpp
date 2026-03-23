#include <Arduino.h>
#include <Wire.h>
#include <display_bsp.h>
#include <Gui.h>
#include <GuiTests.h>
#include <SDCardTests.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <app_message.h>

#include <InputKeyManager.h>
#include <StateManager.h>

#define ENABLE_GUI_TESTS 0
#define ENABLE_SDCARD_TESTS 0

// SPI 接口的反射 LCD 控制器（CS=12, DC=11, RST=5, SDA=40, SCL=41, 400×300）
DisplayPort RlcdPort(12, 11, 5, 40, 41, 400, 300);

// GUI 渲染层，持有帧缓冲，最终通过 RlcdPort 刷屏
Gui gui(&RlcdPort, 400, 300);

// 全局消息队列：后台任务 / ISR → loop() 的唯一数据通道
// 容量 16 条 AppMessage，在 setup() 中创建后不再改变
QueueHandle_t g_msgQueue = nullptr;

// I2C 总线互斥锁：RTC / SHTC3 / Codec 等任务共享 Wire，需加锁保护
SemaphoreHandle_t g_i2cMutex = nullptr;

// 按键驱动：KEY1=GPIO18，KEY2=GPIO0（低电平有效，内部上拉）
// 中断 + FreeRTOS 软件定时器去抖，产生 KeyEvent 后回调注册的监听器
InputKeyManager   keyManager;

// 状态机调度器：维护当前状态，派发消息与按键事件，执行状态切换
// 子状态实例由 stateManager.beginWithStates() 在内部创建与持有
StateManager      stateManager;

// ── 触摸中断（预留，待硬件引脚确认后启用）──────────────────────────────
// 触摸屏中断引脚，-1 表示当前未接入
static constexpr int kTouchIntPin = -1;

// 触摸中断服务程序（ISR）：将触摸事件写入消息队列
// IRAM_ATTR 确保 ISR 代码常驻 IRAM，避免 Cache Miss 导致的执行延迟
void IRAM_ATTR onTouchISR() {
    if (g_msgQueue == nullptr) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    AppMessage msg;
    msg.type    = MSG_TOUCH_EVENT;
    msg.touch.x = 0;
    msg.touch.y = 0;
    xQueueSendFromISR(g_msgQueue, &msg, &xHigherPriorityTaskWoken);
    // 若唤醒了更高优先级任务，立即触发上下文切换
    if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

// ── setup：一次性硬件与软件初始化 ────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // I2C 总线（RTC PCF85063 + 温湿度 SHTC3 + 音频 codec 共用）
    Wire.begin(13, 14);

    // 创建 I2C 互斥锁（必须在任何后台任务启动之前）
    g_i2cMutex = xSemaphoreCreateMutex();
    if (g_i2cMutex == nullptr) {
        Serial.println("创建 I2C 互斥锁失败！");
        while (1) delay(100);
    }

    // 初始化反射 LCD，清屏并刷新一次确保白底显示
    RlcdPort.RLCD_Init();
    gui.setForegroundColor(ColorBlack);
    gui.setBackgroundColor(ColorWhite);
    gui.clear();
    gui.display();

    // 创建全局消息队列（容量 16），后台任务和 ISR 通过它向 loop() 传递数据
    g_msgQueue = xQueueCreate(16, sizeof(AppMessage));
    if (g_msgQueue == nullptr) {
        Serial.println("创建消息队列失败！");
        while (1) delay(100);
    }

    // 配置电池电压 ADC 引脚衰减（GPIO4，量程 0~3.3V）
    analogSetPinAttenuation(4, ADC_11db);

    // 按键管理器初始化，注册监听器将 KeyEvent 转发给状态机
    keyManager.begin();
    keyManager.addListener([](const KeyEvent& e) {
        stateManager.dispatchKeyEvent(e);
    });

    // 创建全部子状态、注册并进入初始状态 MAIN_UI（三步合一）
    // 各子状态所需硬件对象（rtc/humiture/wifiConfig/pomodoro/audioCodec）
    // 已内化为各自状态的值成员，main.cpp 只需传入共用的 Gui 引用
    stateManager.beginWithStates(gui);

#if ENABLE_GUI_TESTS
    // GUI 测试：验证绘图原语和字体渲染，完成后延迟 5 秒并清屏
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

#if ENABLE_SDCARD_TESTS
    SDCardTests::runAllTests();
    delay(3000);
#endif
}

// ── loop：UI Looper（运行在 Arduino 主任务，Core 1）────────────────────────
// 职责：驱动当前状态 tick、消费消息队列、统一刷屏
// 规则：gui.display() 是唯一刷屏点，禁止在其他任何地方调用

void loop() {
    // 队列创建前保护（理论上不会到达，但防止 FreeRTOS 调度异常）
    if (g_msgQueue == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    // 从队列取一条消息（阻塞至多 30ms），转发给当前激活状态处理
    // 30ms 超时确保即使无消息，刷屏频率也稳定在 ~30 fps
    AppMessage msg;
    if (xQueueReceive(g_msgQueue, &msg, pdMS_TO_TICKS(30)) == pdTRUE) {
        stateManager.dispatch(msg);
    }

    gui.display();
}
