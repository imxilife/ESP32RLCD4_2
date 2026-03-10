#pragma once

#include <stdint.h>
#include <functional>

enum class KeyId     : uint8_t { KEY1 = 0, KEY2 = 1 };
enum class KeyAction : uint8_t { DOWN, UP, LONG_PRESS, LONG_REPEAT };

struct KeyEvent {
    KeyId     id;
    KeyAction action;
};

using KeyListener = std::function<void(const KeyEvent&)>;
