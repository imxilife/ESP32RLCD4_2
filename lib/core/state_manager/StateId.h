#pragma once

#include <stdint.h>

enum class StateId : uint8_t {
    LAUNCH       = 0,   // 启动后首页
    MAIN_UI      = 1,
    POMODORO     = 2,
    MUSIC_PLAYER = 3,
    FONT_TEST    = 4,
    XZAI         = 5,
    BLUETOOTH    = 6,   // 蓝牙 A2DP 播放
    OTA          = 7,
    FONT_BIN_TEST = 8,
    STATE_COUNT  = 9,
};
