#pragma once

#include "StateId.h"
#include "AbstractState.h"
#include <app_message.h>
#include <KeyEvent.h>

// 状态机管理器：负责子状态注册、状态迁移和事件分发
// 职责边界：只做调度，不含任何硬件操作
class StateManager {
public:
    // 注册状态实例（StateManager 不拥有生命周期，由调用方保证存活）
    void registerState(StateId id, AbstractState* state);

    // 启动：设置初始状态并触发 onEnter
    void begin(StateId initialState);

    // 将队列消息转发给当前激活状态的 onMessage；
    // 处理完毕后检查是否有待执行的状态迁移
    void dispatch(const AppMessage& msg);

    // 将按键事件转发给当前激活状态的 onKeyEvent（由 InputKeyManager 回调调用）
    void dispatchKeyEvent(const KeyEvent& event);

    // 请求切换状态（由 AbstractState 子类调用）；
    // 实际切换延迟到当前 dispatch/dispatchKeyEvent 调用返回后执行
    void requestTransition(StateId id);

    StateId currentStateId() const { return currentId_; }

private:
    AbstractState* states_[static_cast<int>(StateId::STATE_COUNT)] = {};
    StateId        currentId_      = StateId::MAIN_UI;
    StateId        pendingId_      = StateId::MAIN_UI;
    bool           transitPending_ = false;

    AbstractState* current() { return states_[static_cast<int>(currentId_)]; }
    void doTransition(StateId newId);
};
