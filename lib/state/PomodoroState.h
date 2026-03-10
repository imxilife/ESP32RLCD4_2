#pragma once

#include <AbstractState.h>
#include <Gui.h>
#include <Pomodoro.h>

// 番茄时钟状态：将 InputKeyManager 的按键事件注入 Pomodoro，
// 接收 MSG_POMODORO_UPDATE 并驱动 UI 渲染
class PomodoroState : public AbstractState {
public:
    PomodoroState(Gui& gui, Pomodoro& pomodoro);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui&      gui_;
    Pomodoro& pomodoro_;
};
