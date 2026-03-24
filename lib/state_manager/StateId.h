#pragma once

#include <stdint.h>

enum class StateId : uint8_t {
    MAIN_UI      = 0,
    POMODORO     = 1,
    MUSIC_PLAYER = 2,
    XZAI         = 3,
    BLUETOOTH    = 4,   // 蓝牙 A2DP 播放
    STATE_COUNT  = 5,
};
