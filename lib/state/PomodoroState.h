#pragma once

#include <AbstractState.h>
#include <Gui.h>
#include <Pomodoro.h>
#include <freertos/timers.h>

// 番茄时钟状态：将 InputKeyManager 的按键事件注入 Pomodoro，
// 接收 MSG_POMODORO_UPDATE 并驱动 UI 渲染。
// Pomodoro 对象作为值成员内化，不再依赖 main.cpp 的全局对象。
// 节拍由内部 FreeRTOS 软件定时器驱动，main.cpp 无需感知。
class PomodoroState : public AbstractState {
public:
    explicit PomodoroState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui&          gui_;
    Pomodoro      pomodoro_;
    TimerHandle_t tickTimer_ = nullptr;

    // FreeRTOS 软件定时器回调：在定时器任务上下文中调用 pomodoro_.update()
    static void onTick(TimerHandle_t xTimer);
};
