#include "BluetoothState.h"
#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>

BluetoothState::BluetoothState(Gui& gui) : gui_(gui) {}

void BluetoothState::onEnter() {
    lastConnected_ = false;

    // ESP32-S3 等平台不支持 A2DP，仅显示提示
    if (!BluetoothAudio::kPlatformSupported) {
        drawUI("BLUETOOTH", "Not Supported");
        Serial.println("[BT State] A2DP requires ESP32 (Classic BT)");
        return;
    }

    drawUI("BLUETOOTH", "Waiting...");

    // 懒初始化：首次进入时启动 A2DP Sink
    if (!btReady_) {
        btReady_ = bt_.begin("ESP32-BOX-BT");
        if (!btReady_) {
            drawUI("BLUETOOTH", "Init Failed");
            Serial.println("[BT State] BluetoothAudio init failed");
        }
    } else {
        bt_.begin("ESP32-BOX-BT");
    }
}

void BluetoothState::onExit() {
    bt_.stop();
}

void BluetoothState::onMessage(const AppMessage& msg) {
    if (!BluetoothAudio::kPlatformSupported) return;

    // 轮询蓝牙连接状态，更新 UI
    bool nowConnected = bt_.isConnected();
    if (nowConnected != lastConnected_) {
        lastConnected_ = nowConnected;
        drawUI("BLUETOOTH", nowConnected ? "Connected" : "Waiting...");
    }
}

void BluetoothState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        requestTransition(StateId::LAUNCH);
    }
    // KEY2 预留
}

void BluetoothState::drawUI(const char* line1, const char* line2) {
    gui_.clear();
    gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);

    static const int kScreenW = 400;
    static const int kScreenH = 300;

    int x1 = (kScreenW - gui_.measureTextWidth(line1)) / 2;
    int y1 = kScreenH / 2 - 30;
    if (x1 < 0) x1 = 0;
    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);

    if (line2) {
        int x2 = (kScreenW - gui_.measureTextWidth(line2)) / 2;
        int y2 = y1 + 28;
        if (x2 < 0) x2 = 0;
        gui_.drawText(x2, y2, line2, ColorBlack, ColorWhite);
    }
}
