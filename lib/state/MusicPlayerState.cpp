#include "MusicPlayerState.h"
#include <StateManager.h>
#include "fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"

MusicPlayerState::MusicPlayerState(Gui& gui)
    : gui_(gui) {}

void MusicPlayerState::onEnter() {
    // 首次进入时初始化音频 codec（once-flag 懒初始化）
    // Wire.begin(13,14) 已在 setup() 中提前调用，时序安全
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        if (!audio_.begin(16000)) {
            Serial.println("[Audio] Codec init failed — MIC echo unavailable");
        } else {
            // Step 1: 验证 ES7210 录音输入链路
            audio_.diagMicInput(500);
            // Step 2: 验证 ES8311 音频输出链路
            audio_.diagSpeakerOutput(500);
        }
    }

    gui_.clear();
    gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    static const int kFontW   = 20;
    static const int kScreenW = 400;
    static const int kScreenH = 300;

    const char* line1 = "MIC ECHO";
    const char* line2 = "KEY1=NEXT";

    int x1 = (kScreenW - (int)strlen(line1) * kFontW) / 2;
    int x2 = (kScreenW - (int)strlen(line2) * kFontW) / 2;
    int y1 = (kScreenH - 20 - 8 - 20 - 8 - 20) / 2;
    int y2 = y1 + 20 + 8;

    if (x1 < 0) x1 = 0;
    if (x2 < 0) x2 = 0;

    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);
    gui_.drawText(x2, y2, line2, ColorBlack, ColorWhite);

    audio_.startEcho();
    drawStatusLine(audio_.isRunning() ? "Running" : "Init failed");
}

void MusicPlayerState::onExit() {
    audio_.stopEcho();
}

void MusicPlayerState::onMessage(const AppMessage& msg) {
    switch (msg.type) {
    }
}

void MusicPlayerState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        requestTransition(StateId::XZAI);
    }
}

void MusicPlayerState::drawStatusLine(const char* text) {
    static const int kScreenW = 400;
    static const int kScreenH = 300;
    static const int kFontW   = 20;
    static const int kLineH   = 20;

    int x = (kScreenW - (int)strlen(text) * kFontW) / 2;
    if (x < 0) x = 0;
    gui_.drawText(x, kScreenH - kLineH - 4, text, ColorBlack, ColorWhite);
}
