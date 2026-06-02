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
    int drawChineseFontTest(int topY);
    void drawChineseFontRow(int topY, const Font* const* fonts);
    void drawEnglishFontTest(int topY);
    int drawEnglishSample(int topY, const char* sizeLabel, const Font* font);
    int drawWrappedAsciiText(int x, int y, int maxWidth,
                             const char* text, const Font* font);
};
