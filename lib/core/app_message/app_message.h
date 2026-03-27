#pragma once

#include <Arduino.h>
#include <device/rtc/RTC85063.h>

// ── Pomodoro 消息 ──────────────────────────────────────────────

enum class PomodoroEvent : uint8_t {
    SETUP,     // 设置界面刷新
    TICK,      // 倒计时 tick（remSec / totalSec）
    FINISHED,  // 倒计时结束，开始闪烁（blinkOn）
    EXIT,      // 退出番茄时钟，切回时钟界面
};

enum class OtaPhase : uint8_t {
    HOME = 0,
    CHECKING,
    CONFIRM,
    UPDATING,
    RESULT,
};

struct OtaStatusMsg {
    OtaPhase phase;
    uint8_t progressPercent;
    bool success;
    char currentVersion[16];
    char targetVersion[16];
    char ip[32];
    char message[64];
};

struct PomodoroMsg {
    PomodoroEvent event;
    union {
        struct { uint8_t hours; uint8_t minutes; uint8_t focus; } setup;
        struct { uint32_t remSec; uint32_t totalSec; }           tick;
        struct { bool blinkOn; }                                  finished;
    };
};

// ── 应用消息类型 ───────────────────────────────────────────────

enum MsgType {
    MSG_RTC_UPDATE,      // payload: RTCTime
    MSG_HUMITURE_UPDATE, // payload: float temp, float hum
    MSG_WIFI_STATUS,     // payload: bool connected
    MSG_WIFI_UI,         // payload: 文本行1/行2；两行均空 = 清除 WiFi UI 切回时钟
    MSG_NTP_SYNC,        // payload: ntpTime（NTP 同步成功，loop 负责写入 RTC）
    MSG_TOUCH_EVENT,     // payload: int x, int y
    MSG_BUTTON_EVENT,    // payload: int btnId
    MSG_BATTERY_UPDATE,  // payload: float voltage（单位 V）
    MSG_POMODORO_UPDATE, // payload: PomodoroMsg
    MSG_BT_STATUS,       // payload: bool connected（蓝牙连接状态变化）
    MSG_OTA_STATUS,      // payload: OtaStatusMsg
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
            float voltage;
        } battery;

        PomodoroMsg pomodoro;

        struct {
            bool connected;
        } btStatus;

        OtaStatusMsg ota;
    };
};
