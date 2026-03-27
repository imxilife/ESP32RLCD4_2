#pragma once

#include "StateId.h"
#include "AbstractState.h"
#include <core/app_message/app_message.h>
#include <core/input_key/KeyEvent.h>

// 前向声明：beginWithStates 参数类型，避免将具体硬件头文件引入本头文件
class Gui;

// 状态机管理器：负责子状态注册、状态迁移和事件分发
// 职责边界：只做调度，不含任何硬件操作
class StateManager {
public:
    // 注册状态实例（StateManager 不拥有生命周期，由调用方保证存活）
    void registerState(StateId id, AbstractState* state);

    // 启动：设置初始状态并触发 onEnter
    void begin(StateId initialState);

    // 创建并注册本项目全部子状态，然后调用 begin(MAIN_UI)。
    // 子状态以 static 局部变量形式存活于程序生命周期，由本方法唯一持有。
    // 在 setup() 中替代逐一构造 + registerState + begin 三段样板代码。
    void beginWithStates(Gui& gui);

    // 将队列消息转发给当前激活状态的 onMessage；
    // 处理完毕后检查是否有待执行的状态迁移
    void dispatch(const AppMessage& msg);

    // 将按键事件转发给当前激活状态的 onKeyEvent（由 InputKeyManager 回调调用）
    void dispatchKeyEvent(const KeyEvent& event);

    // 请求切换状态（由 AbstractState 子类调用）；
    // 实际切换延迟到当前 dispatch/dispatchKeyEvent 调用返回后执行
    void requestTransition(StateId id);

    StateId currentStateId() const { return currentId_; }

    // 每帧调用当前状态的 tick()（驱动 CarouselState 动画等）
    void tickCurrentState();

private:
    AbstractState* states_[static_cast<int>(StateId::STATE_COUNT)] = {};
    StateId        currentId_      = StateId::MAIN_UI;
    StateId        pendingId_      = StateId::MAIN_UI;
    bool           transitPending_ = false;

    AbstractState* current() { return states_[static_cast<int>(currentId_)]; }
    void doTransition(StateId newId);

    // 状态切换后吞掉下一次 dispatchKeyEvent，防止同时按键事件泄漏到新状态
    bool swallowNextKey_ = false;
};
