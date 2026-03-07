#pragma once

#include <Arduino.h>
#include <RTC85063.h>

// 应用内消息类型
enum MsgType {
    MSG_RTC_UPDATE,      // payload: RTCTime
    MSG_HUMITURE_UPDATE, // payload: float temp, float hum
    MSG_WIFI_STATUS,     // payload: bool connected
    MSG_WIFI_UI,         // payload: 文本行1/行2，用于 WiFi 配网过程提示
    MSG_NTP_SYNC,        // payload: ntpTime（NTP 同步成功，loop 负责写入 RTC）
    MSG_TOUCH_EVENT,     // payload: int x, int y
    MSG_BUTTON_EVENT,    // payload: int btnId
    MSG_BATTERY_UPDATE,  // payload: float voltage（单位 V）
};

// WiFi UI 文本最大长度（UTF-8 字节数上限）
static const size_t WIFI_UI_LINE_MAX = 64;

// 应用内统一消息结构
struct AppMessage {
    MsgType type;

    union {
        RTCTime rtcTime;

        struct {
            float temp;
            float hum;
        } humiture;

        struct {
            bool connected;
        } wifi;

        struct {
            char line1[WIFI_UI_LINE_MAX];
            char line2[WIFI_UI_LINE_MAX];
        } wifiUi;

        struct {
            int x;
            int y;
        } touch;

        struct {
            int btnId;
        } button;

        struct {
            uint16_t year;
            uint8_t  month, day, hour, minute, second, weekday;
        } ntpTime;

        struct {
            float voltage; // 换算后电池电压（V）
        } battery;
    };
};
