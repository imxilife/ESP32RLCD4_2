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
    void drawCenteredText(int y, const char* text, const Font* font,
                          uint8_t fg = ColorBlack, uint8_t bg = ColorWhite);
    void drawButton(int x, int y, int w, int h, const char* label, bool filled);
    void drawHintBar(const char* left, const char* right);
    void drawInfoCard(int x, int y, int w, int h, const char* title, const char* value);
    void formatIpDisplay(char* out, size_t outSize) const;
    void drawProgressBar(int y, uint8_t percent);
};
