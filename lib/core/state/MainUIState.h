#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>
#include <device/rtc/RTC85063.h>
#include <device/humiture/Humiture.h>
#include <core/app_message/app_message.h>

// 主界面状态：时钟显示、温湿度、WiFi/NTP 状态提示
// rtc / humiture / wifiConfig 作为值成员内化，不再依赖 main.cpp 的全局对象
class MainUIState : public AbstractState {
public:
    explicit MainUIState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui&       gui_;
    RTC85063   rtc_;
    Humiture   humiture_;

    void handleHumiture(float temp, float hum);
};
