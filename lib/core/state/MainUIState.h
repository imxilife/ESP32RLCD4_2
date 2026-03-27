#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>
#include <device/rtc/RTC85063.h>
#include <device/humiture/Humiture.h>
#include <features/network/WiFiConfig.h>
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
    WiFiConfig wifiConfig_;
    bool       wifiUiVisible_ = false;

    // WiFiConfig 回调（静态方法，通过 extern g_msgQueue 投递消息）
    static void wifiUiMessageHandler(const char* line1, const char* line2);
    static void ntpSyncHandler(uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute, uint8_t second,
                               uint8_t weekday);

    void handleHumiture(float temp, float hum);
    void handleWifiUi(const AppMessage& msg);

    static int utf8Count(const char* s);
};
