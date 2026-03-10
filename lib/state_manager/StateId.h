#pragma once

#include <stdint.h>

enum class StateId : uint8_t {
    MAIN_UI      = 0,
    POMODORO     = 1,
    MUSIC_PLAYER = 2,
    XZAI         = 3,
    STATE_COUNT  = 4,
};
