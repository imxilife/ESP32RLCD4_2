#pragma once

#include <core/input_key/KeyEvent.h>
#include <device/display/display_bsp.h>

class Gui;

class View {
public:
    int x = 0, y = 0;
    int width = 0, height = 0;
    uint8_t bgColor = ColorWhite;
    uint8_t fgColor = ColorBlack;
    bool visible = true;

    virtual ~View() = default;
    virtual void draw(Gui& gui) = 0;

    using OnClickCallback = void (*)(View* view);
    void setOnClickListener(OnClickCallback cb) { onClick_ = cb; }

    bool handleKeyEvent(const KeyEvent& event) {
        if (event.action == KeyAction::DOWN) {
            pressed_ = true;
            return true;
        }
        if (event.action == KeyAction::UP && pressed_) {
            pressed_ = false;
            if (onClick_) onClick_(this);
            return true;
        }
        return false;
    }

protected:
    OnClickCallback onClick_ = nullptr;
    bool pressed_ = false;
};
