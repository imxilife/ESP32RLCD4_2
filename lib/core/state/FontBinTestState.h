#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>

/**
 * FontBinTestState
 *
 * Dedicated diagnostics page for the SPIFFS-backed font24.bin parser.
 * Keeping this separate from FontTestState avoids mixing static C-array font
 * comparison with filesystem font verification.
 */
class FontBinTestState : public AbstractState {
public:
    explicit FontBinTestState(Gui& gui);

    void onEnter() override;
    void onExit() override;
    void onMessage(const AppMessage& msg) override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui& gui_;

    void drawScreen();
    void drawSectionFrame(int y, const char* title);
    void drawCenteredLine(int x, int y, int w, const char* text, const Font* font);
};
