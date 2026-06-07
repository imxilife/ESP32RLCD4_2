#include "MainUIState.h"

#include <core/app_tasks/app_tasks.h>
#include <core/state_manager/StateManager.h>
#include <device/display/display_bsp.h>
#include <features/network/NetworkService.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ui/gui/fonts/FontManager.h>
#include <ui/views/clock/ui_clock.h>
#include <cstdio>

MainUIState::MainUIState(Gui& gui)
    : gui_(gui) {}

void MainUIState::onEnter() {
    static bool tasksStarted = false;
    if (!tasksStarted) {
        tasksStarted = true;
        xTaskCreate(humitureTask, "humTask", 2048, &humiture_, 1, nullptr);
        startBatteryTaskOnce();
        Serial.println("[MainUI] background tasks started");
    }

    gui_.clear();
    resetClockState();
}

void MainUIState::onExit() {}

void MainUIState::onMessage(const AppMessage& msg) {
    switch (msg.type) {
    case MSG_RTC_UPDATE:
        handleRtcUpdate(msg.rtcTime);
        break;

    case MSG_HUMITURE_UPDATE:
        handleHumiture(msg.humiture.temp, msg.humiture.hum);
        break;

    case MSG_WIFI_UI:
        Serial.printf("[MainUI] WiFi UI: %s | %s\n",
                      msg.wifiUi.line1,
                      msg.wifiUi.line2);
        break;

    case MSG_WIFI_STATUS:
        Serial.printf("[MainUI] WiFi status: connected=%s\n",
                      msg.wifi.connected ? "true" : "false");
        break;

    case MSG_NTP_SYNC: {
        rtc_.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                     msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                     msg.ntpTime.weekday);
        Serial.println("[MainUI] NTP time written to RTC");
        gui_.clear();

        RTCTime t;
        t.year = msg.ntpTime.year;
        t.month = msg.ntpTime.month;
        t.day = msg.ntpTime.day;
        t.hour = msg.ntpTime.hour;
        t.minute = msg.ntpTime.minute;
        t.second = msg.ntpTime.second;
        t.weekday = msg.ntpTime.weekday;
        resetClockState();
        handleRtcUpdate(t);
        break;
    }

    case MSG_BATTERY_UPDATE:
        Serial.printf("[MainUI] battery voltage: %.2f V\n", msg.battery.voltage);
        break;

    default:
        break;
    }
}

void MainUIState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        requestTransition(StateId::LAUNCH);
    }
}

void MainUIState::handleHumiture(float temperature, float humidity) {
    const Font* font = FontManager::instance().font(FontId::EnMain);
    if (font == nullptr) return;

    constexpr int kMargin = 10;
    const int fontH = font->lineHeight > 0 ? font->lineHeight : 18;
    const int y = 300 - fontH - 16;
    gui_.fillRect(0, y - 4, 400, fontH + 8, ColorWhite);
    gui_.setFont(font);

    char tempBuf[24];
    char humBuf[24];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f\xe2\x84\x83", temperature);
    snprintf(humBuf, sizeof(humBuf), "%.1f%%", humidity);

    gui_.drawText(kMargin, y, tempBuf, ColorBlack, ColorWhite);
    const int humW = gui_.measureTextWidth(humBuf, font);
    gui_.drawText(400 - kMargin - humW, y, humBuf, ColorBlack, ColorWhite);
}
