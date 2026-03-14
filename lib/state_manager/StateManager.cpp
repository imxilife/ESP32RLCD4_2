#include "StateManager.h"
#include <Arduino.h>

// 具体子状态头文件（依赖各自内化，StateManager 只需知道子状态类型）
// 硬件头文件由子状态各自 #include，StateManager 无需再引入
#include <Gui.h>
#include <MainUIState.h>
#include <PomodoroState.h>
#include <MusicPlayerState.h>
#include <XZAIState.h>

// ── 1. 注册 ────────────────────────────────────────────────────────────────
// 注册一个子状态到状态表。
// 将 state 存入以 id 为下标的槽位，并反向设置 state->manager_ 指针，
// 使子状态可通过 requestTransition() 回调管理器。
void StateManager::registerState(StateId id, AbstractState* state) {
    int idx = static_cast<int>(id);
    states_[idx] = state;
    state->manager_ = this;
}

// ── 2. 启动 ────────────────────────────────────────────────────────────────
// 初始化状态机，进入初始状态。
// 重置所有切换标志，然后调用初始状态的 onEnter()。
// 必须在所有 registerState() 调用之后、loop() 开始前调用一次。
void StateManager::begin(StateId initialState) {
    currentId_      = initialState;
    pendingId_      = initialState;
    transitPending_ = false;
    if (current()) current()->onEnter();
    Serial.printf("[StateManager] start: State %d\n", (int)initialState);
}

// ── 3. 按键派发 ────────────────────────────────────────────────────────────
// 将一个按键事件派发给当前状态的 onKeyEvent()。
// 若 swallowNextKey_ 为 true（状态切换刚发生），则丢弃本次事件，
// 防止切换边界上的遗留按键被新状态误处理。
// 派发完成后立即检查并执行挂起的状态切换。
void StateManager::dispatchKeyEvent(const KeyEvent& event) {
    if (swallowNextKey_) {
        swallowNextKey_ = false;
        return;
    }
    if (current()) current()->onKeyEvent(event);
    if (transitPending_) {
        transitPending_ = false;
        doTransition(pendingId_);
    }
}

// ── 4. 消息派发 ────────────────────────────────────────────────────────────
// 将一条 AppMessage 派发给当前状态的 onMessage()。
// 派发完成后立即检查并执行挂起的状态切换，确保切换发生在
// 消息处理结束、下一帧渲染之前。
void StateManager::dispatch(const AppMessage& msg) {
    if (current()) current()->onMessage(msg);
    if (transitPending_) {
        transitPending_ = false;
        doTransition(pendingId_);
    }
}

// ── 5. 请求切换 ────────────────────────────────────────────────────────────
// 由子状态调用，申请切换到指定状态。
// 只记录目标 id 并置位标志，实际切换延迟到当前
// dispatch() / dispatchKeyEvent() 返回前执行，避免在回调中途销毁当前状态。
void StateManager::requestTransition(StateId id) {
    pendingId_      = id;
    transitPending_ = true;
}

// ── 6. 执行切换 ────────────────────────────────────────────────────────────
// 执行实际的状态切换：先调用当前状态的 onExit()，
// 更新 currentId_，置位 swallowNextKey_ 以吞掉切换边界按键，
// 再调用新状态的 onEnter()。
void StateManager::doTransition(StateId newId) {
    AbstractState* cur = current();
    if (cur) cur->onExit();

    currentId_      = newId;
    swallowNextKey_ = true;  // 吞掉切换边界上的遗留按键事件

    AbstractState* next = current();
    if (next) next->onEnter();

    Serial.printf("[StateManager] → State %d\n", (int)newId);
}

// ── 7. 一键初始化（替代 main.cpp 中的状态样板代码）──────────────────────
// 以 static 局部变量创建全部子状态，生命周期与程序相同。
// 各状态所需的硬件对象（rtc/humiture/wifiConfig/pomodoro/audioCodec）
// 已内化为各自状态的值成员，StateManager 无需再持有或传递这些依赖。
// 完成注册后调用 begin(MAIN_UI) 触发初始状态的 onEnter。
void StateManager::beginWithStates(Gui& gui) {
    static MainUIState      stateMainUI(gui);
    static PomodoroState    statePomodoro(gui);
    static MusicPlayerState stateMusic(gui);
    static XZAIState        stateXzai(gui);

    registerState(StateId::MAIN_UI,      &stateMainUI);
    registerState(StateId::POMODORO,     &statePomodoro);
    registerState(StateId::MUSIC_PLAYER, &stateMusic);
    registerState(StateId::XZAI,         &stateXzai);

    begin(StateId::MAIN_UI);
}
