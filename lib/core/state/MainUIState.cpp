#include "MainUIState.h"
#include <core/state_manager/StateManager.h>
#include <ui/views/clock/ui_clock.h>
#include <core/app_tasks/app_tasks.h>
#include <device/display/display_bsp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ui/gui/fonts/Font_chinese_Oswald_Light_28_40.h>
#include <features/network/NetworkService.h>

MainUIState::MainUIState(Gui& gui)
    : gui_(gui) {}

// ── 状态生命周期 ──────────────────────────────────────────────

void MainUIState::onEnter() {
    // 后台数据任务：只在首次进入主界面时启动一次
    // pvParameters 传入各自对象指针，任务内部 cast 取用，消除 extern 依赖
    static bool tasksStarted = false;
    if (!tasksStarted) {
        tasksStarted = true;
        xTaskCreate(humitureTask, "humTask",  2048, &humiture_, 1, nullptr);
        xTaskCreate(batteryTask,  "battTask", 4096, nullptr,    1, nullptr);
        Serial.println("[MainUI] 后台任务已启动");
    }

    gui_.clear();
    resetClockState();
}

void MainUIState::onExit() {}

// ── 消息处理 ──────────────────────────────────────────────────

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

    case MSG_NTP_SYNC:
        // 写入 RTC（无论当前是否显示 WiFi UI 都要写入）
        rtc_.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                     msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                     msg.ntpTime.weekday);
        Serial.println("[MainUI] 时间已写入 RTC");
        gui_.clear();
        {
            RTCTime t;
            t.year    = msg.ntpTime.year;
            t.month   = msg.ntpTime.month;
            t.day     = msg.ntpTime.day;
            t.hour    = msg.ntpTime.hour;
            t.minute  = msg.ntpTime.minute;
            t.second  = msg.ntpTime.second;
            t.weekday = msg.ntpTime.weekday;
            resetClockState();
            handleRtcUpdate(t);
        }
        break;

    case MSG_BATTERY_UPDATE:
        Serial.printf("[MainUI] 电池电压: %.2f V\n", msg.battery.voltage);
        break;

    default:
        break;
    }
}

// ── 按键处理 ──────────────────────────────────────────────────

void MainUIState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        requestTransition(StateId::LAUNCH);
    }
}

// ── 温湿度绘制（从 main.cpp 迁移）────────────────────────────

void MainUIState::handleHumiture(float temperature, float humidity) {
    static const int kAdvX     = 28;
    static const int kFontH    = 40;
    static const int kY        = 300 - kFontH - 16;
    static const int kMargin   = 10;
    static const int kDotR     = 3;
    static const int kDotSep   = 4;
    static const int kDotZoneW = 2 * (kDotSep + kDotR);
    static const int kDotBaseY = kY + 33;
    static const int kDegR     = 4;
    static const int kDegOffX  = 0;
    static const int kDegOffY  = 6;

    gui_.fillRect(0, kY - 4, 400, kFontH + 8, ColorWhite);
    gui_.setFont(&kFont_chinese_Oswald_Light_28_40);

    char numStr[16], intBuf[8], fracBuf[8];

    // 温度：左对齐 "23.0" → "23" + circle + "0C"
    snprintf(numStr, sizeof(numStr), "%.1f", temperature);
    {
        char *dot = strchr(numStr, '.');
        int   il  = dot ? (int)(dot - numStr) : (int)strlen(numStr);
        strncpy(intBuf, numStr, il); intBuf[il] = '\0';
        snprintf(fracBuf, sizeof(fracBuf), "%sC", (dot && dot[1]) ? dot + 1 : "0");
    }
    {
        int ix  = kMargin;
        int dcx = ix + (int)strlen(intBuf) * kAdvX + kDotSep + kDotR;
        int fx  = dcx + kDotR + kDotSep;
        gui_.drawText(ix,  kY, intBuf,  ColorBlack, ColorWhite);
        gui_.fillCircle(dcx, kDotBaseY, kDotR, ColorBlack);
        gui_.drawText(fx,  kY, fracBuf, ColorBlack, ColorWhite);
        int cCellX = fx + ((int)strlen(fracBuf) - 1) * kAdvX;
        gui_.drawCircle(cCellX + kDegOffX, kY + kDegOffY, kDegR, ColorBlack);
    }

    // 湿度：右对齐 "56.8" → "56" + circle + "8%"
    snprintf(numStr, sizeof(numStr), "%.1f", humidity);
    {
        char *dot = strchr(numStr, '.');
        int   il  = dot ? (int)(dot - numStr) : (int)strlen(numStr);
        strncpy(intBuf, numStr, il); intBuf[il] = '\0';
        snprintf(fracBuf, sizeof(fracBuf), "%s%%", (dot && dot[1]) ? dot + 1 : "0");
    }
    {
        int intW = (int)strlen(intBuf) * kAdvX;
        int frW  = (int)strlen(fracBuf) * kAdvX;
        int ix   = 400 - kMargin - intW - kDotZoneW - frW;
        int dcx  = ix + intW + kDotSep + kDotR;
        int fx   = dcx + kDotR + kDotSep;
        gui_.drawText(ix,  kY, intBuf,  ColorBlack, ColorWhite);
        gui_.fillCircle(dcx, kDotBaseY, kDotR, ColorBlack);
        gui_.drawText(fx,  kY, fracBuf, ColorBlack, ColorWhite);
    }
}

// ── UTF-8 字符计数 ────────────────────────────────────────────
