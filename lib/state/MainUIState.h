#pragma once

#include <AbstractState.h>
#include <Gui.h>
#include <RTC85063.h>
#include <WiFiConfig.h>
#include <app_message.h>

// 主界面状态：时钟显示、温湿度、WiFi/NTP 状态提示
// 持有 RTC 引用，负责 NTP 同步后的时间写入（MSG_NTP_SYNC）
class MainUIState : public AbstractState {
public:
    MainUIState(Gui& gui, RTC85063& rtc);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

    // 向 WiFiConfig 注册 UI 和 NTP 回调（在 setup() 中 g_msgQueue 创建后调用）
    void registerCallbacks(WiFiConfig& wifiConfig);

private:
    Gui&      gui_;
    RTC85063& rtc_;
    bool      wifiUiVisible_ = false;

    // WiFiConfig 回调（静态方法，通过 extern g_msgQueue 投递消息）
    static void wifiUiMessageHandler(const char* line1, const char* line2);
    static void ntpSyncHandler(uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute, uint8_t second,
                               uint8_t weekday);

    void handleHumiture(float temp, float hum);
    void handleWifiUi(const AppMessage& msg);

    static int utf8Count(const char* s);
};
