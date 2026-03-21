#include "MusicPlayerState.h"
#include <StateManager.h>
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"

MusicPlayerState::MusicPlayerState(Gui& gui)
    : gui_(gui) {}

void MusicPlayerState::onEnter() {
    // 首次进入时初始化音频 codec（once-flag 懒初始化）
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        if (!audio_.begin(16000)) {
            Serial.println("[Audio] Codec init failed");
        }
    }

    drawUI("KEY2=REC");
}

void MusicPlayerState::onExit() {
    audio_.stop();
}

void MusicPlayerState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void MusicPlayerState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        requestTransition(StateId::XZAI);
    } else if (event.id == KeyId::KEY2) {
        if (!audio_.isRunning()) {
            drawUI("Recording...");
            audio_.startRecordPlay(3000);
        }
    }
}

void MusicPlayerState::drawUI(const char* status) {
    gui_.clear();
    gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    static const int kFontW   = 20;
    static const int kScreenW = 400;
    static const int kScreenH = 300;

    const char* line1 = "REC PLAY";
    int x1 = (kScreenW - (int)strlen(line1) * kFontW) / 2;
    int y1 = kScreenH / 2 - 30;
    if (x1 < 0) x1 = 0;
    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);

    int x2 = (kScreenW - (int)strlen(status) * kFontW) / 2;
    int y2 = y1 + 28;
    if (x2 < 0) x2 = 0;
    gui_.drawText(x2, y2, status, ColorBlack, ColorWhite);
}
