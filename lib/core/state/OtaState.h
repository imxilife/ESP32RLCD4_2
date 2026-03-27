#pragma once

#include <core/state_manager/AbstractState.h>
#include <features/ota/OtaService.h>
#include <ui/gui/Gui.h>

class OtaState : public AbstractState {
public:
    explicit OtaState(Gui& gui);

    void onEnter() override;
    void onExit() override;
    void onMessage(const AppMessage& msg) override;
    void onKeyEvent(const KeyEvent& event) override;
    void tick() override;

private:
    Gui& gui_;
    OtaService ota_;
    OtaStatusMsg status_ = {};
    uint32_t phaseSinceMs_ = 0;

    void draw();
    void drawCenteredText(int y, const char* text);
    void drawProgressBar(int y, uint8_t percent);
};
