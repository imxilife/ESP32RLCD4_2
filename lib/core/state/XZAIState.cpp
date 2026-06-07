#include "XZAIState.h"
#include <core/state_manager/StateManager.h>
#include <device/display/display_bsp.h>
#include <ui/gui/fonts/FontManager.h>

XZAIState::XZAIState(Gui& gui) : gui_(gui) {}

void XZAIState::onEnter() {
    gui_.clear();

    static const int kScreenW = 400;
    static const int kScreenH = 300;

    const char* line1 = "XZAI";
    const char* line2 = "（开发中）";

    const Font* enFont = FontManager::instance().font(FontId::EnMain);
    const Font* zhFont = FontManager::instance().font(FontId::ZhMain);
    int x1 = (kScreenW - gui_.measureTextWidth(line1, enFont)) / 2;
    int x2 = (kScreenW - gui_.measureTextWidth(line2, zhFont)) / 2;
    int y1 = (kScreenH - 18 - 8 - 24) / 2;
    int y2 = y1 + 18 + 8;

    if (x1 < 0) x1 = 0;
    if (x2 < 0) x2 = 0;

    gui_.setFont(enFont);
    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);
    gui_.setFont(zhFont);
    gui_.drawText(x2, y2, line2, ColorBlack, ColorWhite);
}

void XZAIState::onExit() {}

void XZAIState::onMessage(const AppMessage& msg) {
    (void)msg; // 存根：忽略所有后台消息
}

void XZAIState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        requestTransition(StateId::LAUNCH);
    }
}
