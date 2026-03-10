#include "StateManager.h"
#include <Arduino.h>

void StateManager::registerState(StateId id, AbstractState* state) {
    int idx = static_cast<int>(id);
    states_[idx] = state;
    state->manager_ = this;
}

void StateManager::begin(StateId initialState) {
    currentId_      = initialState;
    pendingId_      = initialState;
    transitPending_ = false;
    if (current()) current()->onEnter();
    Serial.printf("[StateManager] start: State %d\n", (int)initialState);
}

void StateManager::dispatch(const AppMessage& msg) {
    if (current()) current()->onMessage(msg);
    if (transitPending_) {
        transitPending_ = false;
        doTransition(pendingId_);
    }
}

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

void StateManager::requestTransition(StateId id) {
    pendingId_      = id;
    transitPending_ = true;
}

void StateManager::doTransition(StateId newId) {
    AbstractState* cur = current();
    if (cur) cur->onExit();

    currentId_      = newId;
    swallowNextKey_ = true;  // 吞掉切换边界上的遗留按键事件

    AbstractState* next = current();
    if (next) next->onEnter();

    Serial.printf("[StateManager] → State %d\n", (int)newId);
}
