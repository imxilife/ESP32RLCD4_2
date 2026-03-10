#pragma once

#include "StateId.h"
#include <app_message.h>
#include <KeyEvent.h>

class StateManager; // 前向声明，避免循环包含

// 所有功能状态的抽象基类
class AbstractState {
public:
    virtual ~AbstractState() = default;

    // 切入此状态时调用一次（清屏、初始化局部状态）
    virtual void onEnter() = 0;

    // 切离此状态时调用一次（清理临时状态）
    virtual void onExit() = 0;

    // 处理来自 g_msgQueue 的后台消息（仅响应本状态关心的类型）
    virtual void onMessage(const AppMessage& msg) = 0;

    // 处理按键事件（由 StateManager 从 InputKeyManager 回调转发）
    virtual void onKeyEvent(const KeyEvent& event) = 0;

protected:
    // 子类调用此方法请求切换到另一状态，切换延迟到当前 dispatch 返回后执行
    void requestTransition(StateId id);

    StateManager* manager_ = nullptr;
    friend class StateManager;
};
