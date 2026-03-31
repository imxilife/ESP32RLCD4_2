#include "OtaState.h"

#include <core/state_manager/StateManager.h>
#include <device/display/display_bsp.h>
#include <esp_system.h>
#include <ui/gui/Font.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

namespace {
constexpr int kScreenW = 400;
constexpr int kScreenH = 300;
constexpr int kBodyFontH = 20;
constexpr int kTitleFontH = 20;
constexpr int kPrimaryBtnW = 220;
constexpr int kPrimaryBtnH = 44;
constexpr int kPrimaryBtnX = (kScreenW - kPrimaryBtnW) / 2;
constexpr int kPrimaryBtnY = 104;
constexpr int kCardX = 40;
constexpr int kCardW = 320;
constexpr int kCardH = 74;
constexpr int kHintBarY = 258;

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
        requestTransition(StateId::LAUNCH);
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

    switch (status_.phase) {
    case OtaPhase::HOME: {
        char versionLine[48];
        char ipLine[32];
        snprintf(versionLine, sizeof(versionLine), "FW %s", status_.currentVersion);
        formatIpDisplay(ipLine, sizeof(ipLine));

        drawCenteredText(18, "OTA", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(66, versionLine, &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawButton(kPrimaryBtnX, kPrimaryBtnY, kPrimaryBtnW, kPrimaryBtnH, "CHECK OTA", true);

        if (status_.ip[0] != '\0') {
            drawInfoCard(kCardX, 170, kCardW, kCardH, "LOCAL UPLOAD", ipLine);
        }

        drawHintBar("KEY1 CHECK", "KEY2 BACK");
        break;
    }
    case OtaPhase::CHECKING:
        drawCenteredText(24, "OTA", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(86, "CHECKING", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(116, status_.message[0] ? status_.message : "PLEASE WAIT",
                         &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawProgressBar(160, 0);
        drawHintBar("BUSY", "PLEASE WAIT");
        break;
    case OtaPhase::CONFIRM: {
        char currentLine[48];
        char targetLine[48];
        snprintf(currentLine, sizeof(currentLine), "CURRENT %s", status_.currentVersion);
        snprintf(targetLine, sizeof(targetLine), "TARGET  %s", status_.targetVersion);
        drawCenteredText(24, "UPDATE", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(88, currentLine, &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(118, targetLine, &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawButton(46, 170, 136, 44, "UPGRADE", true);
        drawButton(218, 170, 136, 44, "CANCEL", false);
        drawHintBar("KEY1 YES", "KEY2 NO");
        break;
    }
    case OtaPhase::UPDATING:
        drawCenteredText(24, "UPDATING", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(88, "KEEP POWER ON", &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(118, status_.message[0] ? status_.message : "WRITING FIRMWARE",
                         &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawProgressBar(162, status_.progressPercent);
        drawHintBar("BUSY", "DO NOT EXIT");
        break;
    case OtaPhase::RESULT:
        drawCenteredText(28, status_.success ? "SUCCESS" : "FAILED",
                         &kFont_ascii_IBMPlexSans_Medium_20_20);
        drawCenteredText(106, status_.message[0] ? status_.message :
                         (status_.success ? "DONE" : "EXIT"),
                         &kFont_ascii_IBMPlexSans_Medium_20_20);
        if (status_.success) {
            drawProgressBar(162, 100);
            drawHintBar("REBOOT", "WAIT");
        } else {
            drawHintBar("RETURN", "WAIT");
        }
        break;
    }
}

void OtaState::drawCenteredText(int y, const char* text) {
    drawCenteredText(y, text, &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);
}

void OtaState::drawCenteredText(int y, const char* text, const Font* font,
                                uint8_t fg, uint8_t bg) {
    if (text == nullptr || font == nullptr) return;
    gui_.setFont(font);
    int x = (kScreenW - gui_.measureTextWidth(text, font)) / 2;
    if (x < 0) x = 0;
    gui_.drawText(x, y, text, fg, bg);
}

void OtaState::drawButton(int x, int y, int w, int h, const char* label, bool filled) {
    if (filled) {
        gui_.fillRoundRect(x, y, w, h, 10, ColorBlack);
    } else {
        gui_.fillRoundRect(x, y, w, h, 10, ColorWhite);
        gui_.drawRoundRect(x, y, w, h, 10, ColorBlack);
    }

    gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
    int labelX = x + (w - gui_.measureTextWidth(label, &kFont_ascii_IBMPlexSans_Medium_20_20)) / 2;
    int labelY = y + (h - kBodyFontH) / 2;
    gui_.drawText(labelX, labelY, label,
                  filled ? ColorWhite : ColorBlack,
                  filled ? ColorBlack : ColorWhite);
}

void OtaState::drawHintBar(const char* left, const char* right) {
    gui_.drawLine(24, kHintBarY - 10, kScreenW - 24, kHintBarY - 10, ColorBlack);
    if (left != nullptr && left[0] != '\0') {
        gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
        gui_.drawText(24, kHintBarY, left, ColorBlack, ColorWhite);
    }
    if (right != nullptr && right[0] != '\0') {
        int rightX = kScreenW - 24 - gui_.measureTextWidth(right, &kFont_ascii_IBMPlexSans_Medium_20_20);
        if (rightX < 0) rightX = 0;
        gui_.setFont(&kFont_ascii_IBMPlexSans_Medium_20_20);
        gui_.drawText(rightX, kHintBarY, right, ColorBlack, ColorWhite);
    }
}

void OtaState::drawInfoCard(int x, int y, int w, int h, const char* title, const char* value) {
    gui_.drawRoundRect(x, y, w, h, 10, ColorBlack);
    drawCenteredText(y + 10, title, &kFont_ascii_IBMPlexSans_Medium_20_20);
    drawCenteredText(y + 40, value, &kFont_ascii_IBMPlexSans_Medium_20_20);
}

void OtaState::formatIpDisplay(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    out[0] = '\0';
    if (status_.ip[0] == '\0') return;

    const char* start = status_.ip;
    if (strncmp(start, "http://", 7) == 0) {
        start += 7;
    }

    strncpy(out, start, outSize - 1);
    out[outSize - 1] = '\0';

    size_t len = strlen(out);
    if (len > 0 && out[len - 1] == '/') {
        out[len - 1] = '\0';
    }
}

void OtaState::drawProgressBar(int y, uint8_t percent) {
    const int barW = 280;
    const int barH = 22;
    const int x = (kScreenW - barW) / 2;
    const int fillW = (barW * percent) / 100;
    char percentText[8];

    gui_.drawRoundRect(x, y, barW, barH, 6, ColorBlack);
    if (fillW > 0) {
        gui_.fillRoundRect(x + 1, y + 1, fillW - 2 > 0 ? fillW - 2 : 1, barH - 2, 5, ColorBlack);
    }

    snprintf(percentText, sizeof(percentText), "%u%%", percent);
    drawCenteredText(y + 34, percentText, &kFont_ascii_IBMPlexSans_Medium_20_20);
}
