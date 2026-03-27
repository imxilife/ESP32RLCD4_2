#include "OtaState.h"

#include <core/state_manager/StateManager.h>
#include <device/display/display_bsp.h>
#include <esp_system.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

namespace {
constexpr int kScreenW = 400;
constexpr int kScreenH = 300;
constexpr int kFontW = 20;
constexpr int kLineH = 20;
constexpr int kBtnW = 180;
constexpr int kBtnH = 40;
constexpr int kBtnX = (kScreenW - kBtnW) / 2;
constexpr int kBtnY = 150;
}

OtaState::OtaState(Gui& gui)
    : gui_(gui) {}

void OtaState::onEnter() {
    memset(&status_, 0, sizeof(status_));
    status_.phase = OtaPhase::HOME;
    strncpy(status_.currentVersion, APP_VERSION, sizeof(status_.currentVersion) - 1);
    phaseSinceMs_ = millis();
    ota_.begin();
    draw();
}

void OtaState::onExit() {
    ota_.end();
}

void OtaState::onMessage(const AppMessage& msg) {
    if (msg.type != MSG_OTA_STATUS) return;

    status_ = msg.ota;
    phaseSinceMs_ = millis();
    draw();
}

void OtaState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (status_.phase == OtaPhase::CHECKING || status_.phase == OtaPhase::UPDATING) {
        return;
    }

    if (status_.phase == OtaPhase::CONFIRM) {
        if (event.id == KeyId::KEY1) {
            ota_.confirmRemoteUpgrade();
        } else if (event.id == KeyId::KEY2) {
            ota_.publishHome();
        }
        return;
    }

    if (status_.phase == OtaPhase::RESULT) {
        return;
    }

    if (event.id == KeyId::KEY1) {
        ota_.startCheck();
    } else if (event.id == KeyId::KEY2) {
        requestTransition(StateId::CAROUSEL);
    }
}

void OtaState::tick() {
    ota_.tick();

    if (status_.phase == OtaPhase::RESULT) {
        uint32_t elapsed = millis() - phaseSinceMs_;
        if (status_.success && elapsed >= 1500) {
            esp_restart();
        } else if (!status_.success && elapsed >= 2000) {
            ota_.publishHome();
        }
    }
}

void OtaState::draw() {
    gui_.clear();
    gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    switch (status_.phase) {
    case OtaPhase::HOME: {
        char versionLine[48];
        snprintf(versionLine, sizeof(versionLine), "Firmware %s", status_.currentVersion);
        drawCenteredText(72, versionLine);

        gui_.fillRoundRect(kBtnX, kBtnY, kBtnW, kBtnH, 8, ColorBlack);
        gui_.drawText(kBtnX + 40, kBtnY + 10, "检测升级", ColorWhite, ColorBlack);

        if (status_.ip[0] != '\0') {
            drawCenteredText(220, "本地上传地址");
            drawCenteredText(248, status_.ip);
        }
        break;
    }
    case OtaPhase::CHECKING:
        drawCenteredText(110, "检测升级中");
        drawCenteredText(138, status_.message[0] ? status_.message : "请稍候");
        drawProgressBar(190, 0);
        break;
    case OtaPhase::CONFIRM: {
        char currentLine[48];
        char targetLine[48];
        snprintf(currentLine, sizeof(currentLine), "当前版本 %s", status_.currentVersion);
        snprintf(targetLine, sizeof(targetLine), "新版本 %s", status_.targetVersion);
        drawCenteredText(90, currentLine);
        drawCenteredText(118, targetLine);
        drawCenteredText(180, "KEY1=升级");
        drawCenteredText(208, "KEY2=取消");
        break;
    }
    case OtaPhase::UPDATING:
        drawCenteredText(100, "当前有固件在升级中");
        drawCenteredText(128, "请勿断电");
        drawProgressBar(190, status_.progressPercent);
        break;
    case OtaPhase::RESULT:
        drawCenteredText(118, status_.message[0] ? status_.message :
                         (status_.success ? "升级成功" : "超时退出"));
        if (status_.success) {
            drawProgressBar(190, 100);
        }
        break;
    }
}

void OtaState::drawCenteredText(int y, const char* text) {
    if (text == nullptr) return;
    int x = (kScreenW - (int)strlen(text) * kFontW) / 2;
    if (x < 0) x = 0;
    gui_.drawText(x, y, text, ColorBlack, ColorWhite);
}

void OtaState::drawProgressBar(int y, uint8_t percent) {
    const int barW = 240;
    const int barH = 20;
    const int x = (kScreenW - barW) / 2;
    const int fillW = (barW * percent) / 100;
    char percentText[8];

    gui_.drawRoundRect(x, y, barW, barH, 6, ColorBlack);
    if (fillW > 0) {
        gui_.fillRoundRect(x + 1, y + 1, fillW - 2 > 0 ? fillW - 2 : 1, barH - 2, 5, ColorBlack);
    }

    snprintf(percentText, sizeof(percentText), "%u%%", percent);
    drawCenteredText(y + 30, percentText);
}
