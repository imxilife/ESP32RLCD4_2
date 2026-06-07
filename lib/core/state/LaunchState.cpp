#include "LaunchState.h"

#include <Arduino.h>
#include <core/app_tasks/app_tasks.h>
#include <features/memo/MemoService.h>
#include <features/pomodoro/Pomodoro.h>
#include <features/voice_assistant/VoiceAssistantService.h>
#include <ui/assets/HomeBitmapAssets.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_18_Var.h>
#include <ui/gui/fonts/Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_24_Var.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSansCondensed_SemiBold_40_64.h>
#include <ui/gui/fonts/Font_chinese_HomeMemo_20_20.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr int kScreenW = 400;
constexpr int kScreenH = 300;

constexpr int kDividerX = 200;
constexpr int kDividerTop = 40;
constexpr int kDividerBottom = 253;

constexpr int kLeftX = 62;
constexpr int kHourY = 72;
constexpr int kMinuteY = 136;
constexpr int kDateY = 216;
constexpr int kLeftTextRight = kDividerX - 10;

constexpr int kStatusY = 8;
constexpr int kBatteryX = kScreenW - 10 - HomeBitmapAssets::kStatusIconW;
constexpr int kWifiX = kBatteryX - HomeBitmapAssets::kStatusIconW - 6;

constexpr int kRightIconX = 222;
constexpr int kRightTextX = 258;
constexpr int kRightTextMaxW = kScreenW - kRightTextX - 24;
constexpr int kWeatherY = 68;
constexpr int kPomodoroY = 126;
constexpr int kMemoY = 184;
constexpr int kPomodoroIconOffsetX = 2 - ((HomeBitmapAssets::kPomodoroIconW - HomeBitmapAssets::kHomeRowIconW) / 2);
constexpr int kPomodoroIconOffsetY = 7 - ((HomeBitmapAssets::kPomodoroIconH - HomeBitmapAssets::kHomeRowIconH) / 2);

constexpr int kVoiceX = 20;
constexpr int kVoiceY = 258;

const Font* kTimeFont = &kFont_ascii_IBMPlexSansCondensed_SemiBold_40_64;
const Font* kValueFont = &kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_24_Var;
const Font* kLabelFont = &kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_18_Var;
const Font* kMemoFont = &kFont_chinese_HomeMemo_20_20;

constexpr const char* kWeekLabels[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

constexpr const char* kMonthLabels[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

String fitTextToWidth(Gui& gui, const char* text, const Font* font, int maxWidth) {
    if (text == nullptr || font == nullptr || maxWidth <= 0) return "";
    if (gui.measureTextWidth(text, font) <= maxWidth) return String(text);

    String result(text);
    while (result.length() > 0) {
        result.remove(result.length() - 1);
        String candidate = result + "...";
        if (gui.measureTextWidth(candidate.c_str(), font) <= maxWidth) return candidate;
    }
    return "";
}
}

LaunchState::LaunchState(Gui& gui)
    : gui_(gui) {}

void LaunchState::onEnter() {
    static bool rtcStarted = false;
    if (!rtcStarted) {
        rtcStarted = true;
        xTaskCreate(rtcTask, "rtcTask", 2048, &rtc_, 2, nullptr);
        Serial.println("[Launch] RTC task started");
    }

    startBatteryTaskOnce();
    voicePhase_ = VoiceAssistantService::phase();
    key2LongPressConsumed_ = false;
    draw();
}

void LaunchState::onExit() {}

void LaunchState::onMessage(const AppMessage& msg) {
    switch (msg.type) {
    case MSG_RTC_UPDATE:
        currentTime_ = msg.rtcTime;
        hasRtc_ = true;
        draw();
        break;

    case MSG_NTP_SYNC:
        handleNtpSync(msg);
        draw();
        break;

    case MSG_WIFI_STATUS:
        wifiConnected_ = msg.wifi.connected;
        draw();
        break;

    case MSG_BATTERY_UPDATE:
        batteryVoltage_ = msg.battery.voltage;
        draw();
        break;

    case MSG_WEATHER_UPDATE:
        weather_ = msg.weather;
        draw();
        break;

    case MSG_MEMO_UPDATE:
    case MSG_POMODORO_UPDATE:
        draw();
        break;

    case MSG_VOICE_ASSISTANT_UPDATE:
        voicePhase_ = msg.voiceAssistant.phase;
        draw();
        break;

    default:
        break;
    }
}

void LaunchState::onKeyEvent(const KeyEvent& event) {
    // KEY2 long press is reserved as a diagnostics shortcut to the SPIFFS font test.
    if (event.id != KeyId::KEY2) return;

    if (event.action == KeyAction::LONG_PRESS) {
        key2LongPressConsumed_ = true;
        Serial.println("[Launch] Enter FontBinTestState from KEY2 long press");
        requestTransition(StateId::FONT_BIN_TEST);
        return;
    }

    if (event.action == KeyAction::UP) {
        if (key2LongPressConsumed_) {
            key2LongPressConsumed_ = false;
            return;
        }
    }
}

void LaunchState::tick() {}

void LaunchState::draw() {
    drawShell();
    drawStatusIcons();
    drawTimeBlock();
    drawRightColumn();
    drawVoiceAssistant();
}

void LaunchState::drawShell() {
    // The physical shell provides the outer border; software renders only the white page.
    gui_.fillRect(0, 0, kScreenW, kScreenH, ColorWhite);
    gui_.drawLine(kDividerX, kDividerTop, kDividerX, kDividerBottom, ColorBlack);
}

void LaunchState::drawStatusIcons() {
    gui_.drawBitmap(kWifiX, kStatusY, HomeBitmapAssets::kStatusIconW, HomeBitmapAssets::kStatusIconH,
                    wifiConnected_ ? HomeBitmapAssets::kWifi : HomeBitmapAssets::kNoWifi,
                    ColorBlack);
    gui_.drawBitmap(kBatteryX, kStatusY, HomeBitmapAssets::kStatusIconW, HomeBitmapAssets::kStatusIconH,
                    batteryIcon(), ColorBlack);
}

void LaunchState::drawTimeBlock() {
    const uint8_t hour = hasRtc_ ? currentTime_.hour : 0;
    const uint8_t minute = hasRtc_ ? currentTime_.minute : 0;

    char buf[8];
    gui_.setFont(kTimeFont);
    snprintf(buf, sizeof(buf), "%02u", hour);
    gui_.drawText(kLeftX, kHourY, buf, ColorBlack, ColorWhite);
    snprintf(buf, sizeof(buf), "%02u", minute);
    gui_.drawText(kLeftX, kMinuteY, buf, ColorBlack, ColorWhite);

    char dateBuf[20];
    formatDate(dateBuf, sizeof(dateBuf));
    gui_.setFont(kLabelFont);
    const int dateW = gui_.measureTextWidth(dateBuf, kLabelFont);
    int dateX = (kDividerX - dateW) / 2;
    if (dateX < 8) dateX = 8;
    if (dateX + dateW > kLeftTextRight) {
        dateX = kLeftTextRight - dateW;
        if (dateX < 8) dateX = 8;
    }
    gui_.drawText(dateX, kDateY, dateBuf, ColorBlack, ColorWhite);
}

void LaunchState::drawRightColumn() {
    drawWeatherRow(kWeatherY);
    drawPomodoroRow(kPomodoroY);
    drawMemoRow(kMemoY);
}

void LaunchState::drawWeatherRow(int y) {
    if (weather_.valid) {
        drawWeatherIcon(kRightIconX, y + 6, weather_.iconCode);
    } else {
        drawHomeRowIcon(kRightIconX, y + 6, HomeBitmapAssets::kWeatherPlaceholder);
    }
    drawTemperature(kRightTextX, y, weather_.temperatureC, weather_.valid);
    drawFittedText(kRightTextX, y + 27, kRightTextMaxW,
                   weather_.valid ? weather_.text : "WEATHER", kLabelFont);
}

void LaunchState::drawPomodoroRow(int y) {
    // 番茄钟使用 24x24 专用取模，减少 20x20 缩放造成的细节粘连。
    gui_.drawBitmap(kRightIconX + kPomodoroIconOffsetX, y + kPomodoroIconOffsetY,
                    HomeBitmapAssets::kPomodoroIconW, HomeBitmapAssets::kPomodoroIconH,
                    HomeBitmapAssets::kPomodoroClock, ColorBlack);

    const Pomodoro::Snapshot snap = PomodoroService::instance().snapshot();
    const uint32_t rem = snap.remSec;
    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu",
             static_cast<unsigned long>(rem / 60),
             static_cast<unsigned long>(rem % 60));

    gui_.setFont(kValueFont);
    gui_.drawText(kRightTextX, y, timeBuf, ColorBlack, ColorWhite);
    drawFittedText(kRightTextX, y + 27, kRightTextMaxW,
                   snap.finished ? "DONE" : "FOCUS", kLabelFont);
}

void LaunchState::drawMemoRow(int y) {
    drawHomeRowIcon(kRightIconX + 3, y + 7, HomeBitmapAssets::kTodo);

    MemoItem memos[2];
    const uint8_t count = hasRtc_ ? MemoService::topToday(currentTime_, memos, 2) : 0;
    if (count == 0) {
        drawChineseWrappedText(kRightTextX, y + 1, kRightTextMaxW, "今日无事，很开心");
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        drawFittedText(kRightTextX, y + 1 + i * 22, kRightTextMaxW, memos[i].text, kMemoFont);
    }
}

void LaunchState::drawVoiceAssistant() {
    drawMicIcon(kVoiceX, kVoiceY - 3);
    gui_.setFont(kLabelFont);
    gui_.drawText(kVoiceX + 24, kVoiceY + 4, "Listening,", ColorBlack, ColorWhite);
}

void LaunchState::drawWeatherIcon(int x, int y, uint16_t iconCode) {
    const uint8_t* bitmap = HomeBitmapAssets::weatherIcon(iconCode);
    if (bitmap == nullptr) return;
    gui_.drawBitmap(x, y, HomeBitmapAssets::kWeatherIconW, HomeBitmapAssets::kWeatherIconH,
                    bitmap, ColorBlack);
}

void LaunchState::drawHomeRowIcon(int x, int y, const uint8_t* bitmap) {
    if (bitmap == nullptr) return;
    gui_.drawBitmap(x, y, HomeBitmapAssets::kHomeRowIconW, HomeBitmapAssets::kHomeRowIconH,
                    bitmap, ColorBlack);
}

void LaunchState::drawMicIcon(int x, int y) {
    gui_.drawRoundRect(x + 6, y, 10, 17, 5, ColorBlack);
    gui_.fillRect(x + 10, y + 17, 2, 7, ColorBlack);
    gui_.drawLine(x + 4, y + 11, x + 4, y + 15, ColorBlack);
    gui_.drawLine(x + 18, y + 11, x + 18, y + 15, ColorBlack);
    gui_.drawLine(x + 4, y + 15, x + 8, y + 20, ColorBlack);
    gui_.drawLine(x + 18, y + 15, x + 14, y + 20, ColorBlack);
    gui_.fillRect(x + 6, y + 24, 12, 2, ColorBlack);
}

void LaunchState::drawTemperature(int x, int y, int16_t temperatureC, bool valid) {
    char tempBuf[8];
    if (valid) {
        snprintf(tempBuf, sizeof(tempBuf), "%d", static_cast<int>(temperatureC));
    } else {
        snprintf(tempBuf, sizeof(tempBuf), "--");
    }

    gui_.setFont(kValueFont);
    gui_.drawText(x, y, tempBuf, ColorBlack, ColorWhite);
    const int tempW = gui_.measureTextWidth(tempBuf, kValueFont);
    gui_.drawCircle(x + tempW + 3, y + 4, 3, ColorBlack);
    gui_.drawText(x + tempW + 9, y, "C", ColorBlack, ColorWhite);
}

void LaunchState::drawFittedText(int x, int y, int maxWidth, const char* text, const Font* font) {
    if (text == nullptr || font == nullptr) return;
    const String fitted = fitTextToWidth(gui_, text, font, maxWidth);
    gui_.setFont(font);
    gui_.drawText(x, y, fitted.c_str(), ColorBlack, ColorWhite);
}

void LaunchState::drawChineseWrappedText(int x, int y, int maxWidth, const char* text) {
    if (text == nullptr) return;

    String line;
    const char* p = text;
    int lineY = y;
    while (*p != '\0' && lineY <= y + 24) {
        const char* charStart = p;
        uint8_t lead = static_cast<uint8_t>(*p);
        size_t len = 1;
        if ((lead & 0xE0) == 0xC0) len = 2;
        else if ((lead & 0xF0) == 0xE0) len = 3;
        else if ((lead & 0xF8) == 0xF0) len = 4;

        String next = line + String(charStart).substring(0, len);
        if (gui_.measureTextWidth(next.c_str(), kMemoFont) > maxWidth && line.length() > 0) {
            gui_.setFont(kMemoFont);
            gui_.drawText(x, lineY, line.c_str(), ColorBlack, ColorWhite);
            line = "";
            lineY += 22;
            continue;
        }

        line = next;
        p += len;
    }

    if (line.length() > 0) {
        gui_.setFont(kMemoFont);
        gui_.drawText(x, lineY, line.c_str(), ColorBlack, ColorWhite);
    }
}

const uint8_t* LaunchState::batteryIcon() const {
    if (batteryVoltage_ <= 0.0f) return HomeBitmapAssets::kBatteryFull;
    if (batteryVoltage_ < 3.55f) return HomeBitmapAssets::kBattery25;
    if (batteryVoltage_ < 3.75f) return HomeBitmapAssets::kBattery50;
    if (batteryVoltage_ < 3.95f) return HomeBitmapAssets::kBattery75;
    return HomeBitmapAssets::kBatteryFull;
}

void LaunchState::formatDate(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    if (!hasRtc_) {
        snprintf(out, outSize, "---, --- --");
        return;
    }

    const uint8_t weekday = currentTime_.weekday < 7 ? currentTime_.weekday : 0;
    const uint8_t monthIndex = (currentTime_.month >= 1 && currentTime_.month <= 12)
                                 ? currentTime_.month - 1
                                 : 0;
    snprintf(out, outSize, "%s, %s %02u",
             kWeekLabels[weekday], kMonthLabels[monthIndex], currentTime_.day);
}

void LaunchState::handleNtpSync(const AppMessage& msg) {
    rtc_.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                 msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                 msg.ntpTime.weekday);
    currentTime_.year = msg.ntpTime.year;
    currentTime_.month = msg.ntpTime.month;
    currentTime_.day = msg.ntpTime.day;
    currentTime_.hour = msg.ntpTime.hour;
    currentTime_.minute = msg.ntpTime.minute;
    currentTime_.second = msg.ntpTime.second;
    currentTime_.weekday = msg.ntpTime.weekday;
    hasRtc_ = true;
    Serial.println("[Launch] NTP time written to RTC");
}
