#pragma once

#include <AbstractState.h>
#include <Gui.h>

class XZAIState : public AbstractState {
public:
    explicit XZAIState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui& gui_;
};
