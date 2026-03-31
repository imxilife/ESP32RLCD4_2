#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>

class FontTestState : public AbstractState {
public:
    explicit FontTestState(Gui& gui);

    void onEnter() override;
    void onExit() override;
    void onMessage(const AppMessage& msg) override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui& gui_;

    void drawScreen();
    void drawFontSection(int y, const char* title, const Font* font);
};
