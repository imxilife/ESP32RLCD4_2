#pragma once

#include <stdint.h>

enum class StateId : uint8_t {
    CAROUSEL     = 0,   // 旋转木马主界面（初始状态）
    MAIN_UI      = 1,
    POMODORO     = 2,
    MUSIC_PLAYER = 3,
    XZAI         = 4,
    BLUETOOTH    = 5,   // 蓝牙 A2DP 播放
    OTA          = 6,
    STATE_COUNT  = 7,
};
