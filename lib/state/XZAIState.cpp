#include "XZAIState.h"
#include <StateManager.h>
#include <display_bsp.h>
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"

XZAIState::XZAIState(Gui& gui) : gui_(gui) {}

void XZAIState::onEnter() {
    gui_.clear();
    gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    static const int kFontW   = 20;
    static const int kScreenW = 400;
    static const int kScreenH = 300;

    const char* line1 = "XZAI";
    const char* line2 = "（开发中）";

    int x1 = (kScreenW - (int)strlen(line1) * kFontW) / 2;
    int x2 = (kScreenW - 4 * kFontW) / 2;
    int y1 = (kScreenH - 20 - 8 - 20) / 2;
    int y2 = y1 + 20 + 8;

    if (x1 < 0) x1 = 0;
    if (x2 < 0) x2 = 0;

    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);
    gui_.drawText(x2, y2, line2, ColorBlack, ColorWhite);
}

void XZAIState::onExit() {}

void XZAIState::onMessage(const AppMessage& msg) {
    (void)msg; // 存根：忽略所有后台消息
}

void XZAIState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        // 切换到蓝牙状态
        requestTransition(StateId::BLUETOOTH);
    }
}
