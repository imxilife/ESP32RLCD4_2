#pragma once

#include <AbstractState.h>
#include <Gui.h>

class MusicPlayerState : public AbstractState {
public:
    explicit MusicPlayerState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui& gui_;
};
