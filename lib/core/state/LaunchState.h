#pragma once

#include <core/app_message/app_message.h>
#include <core/state_manager/AbstractState.h>
#include <device/rtc/RTC85063.h>
#include <ui/gui/Font.h>
#include <ui/gui/Gui.h>

class LaunchState : public AbstractState {
public:
    explicit LaunchState(Gui& gui);

    void onEnter() override;
    void onExit() override;
    void onMessage(const AppMessage& msg) override;
    void onKeyEvent(const KeyEvent& event) override;
    void tick() override;

private:
    struct TimeRollAnimation {
        bool active = false;
        uint32_t startMs = 0;
        uint32_t durationMs = 1000;
        char fromDigits[5] = "0000";
        char toDigits[5] = "0000";
    };

    Gui& gui_;
    RTC85063 rtc_;
    RTCTime currentTime_ = {};
    char visibleTimeDigits_[5] = "0000";
    TimeRollAnimation timeRoll_;
    bool hasRtc_ = false;
    bool wifiConnected_ = false;
    float batteryVoltage_ = 0.0f;
    WeatherStatusMsg weather_ = {};
    VoiceAssistantPhase voicePhase_ = VoiceAssistantPhase::IDLE;
    bool key2LongPressConsumed_ = false;

    void draw();
    void drawShell();
    void drawStatusIcons();
    void drawTimeBlock();
    void drawRightColumn();
    void drawWeatherRow(int y);
    void drawPomodoroRow(int y);
    void drawMemoRow(int y);
    void drawVoiceAssistant();

    void drawWeatherIcon(int x, int y, uint16_t iconCode);
    void drawHomeRowIcon(int x, int y, const uint8_t* bitmap);
    void drawMicIcon(int x, int y);
    void drawTemperature(int x, int y, int16_t temperatureC, bool valid);
    void drawFittedText(int x, int y, int maxWidth, const char* text, const Font* font);
    void drawChineseWrappedText(int x, int y, int maxWidth, const char* text);

    const uint8_t* batteryIcon() const;
    void formatDate(char* out, size_t outSize) const;
    void applyTimeUpdate(const RTCTime& nextTime);
    void startTimeRollAnimation(const char* fromDigits, const char* toDigits);
    void handleNtpSync(const AppMessage& msg);
};
