#pragma once

#include <stdint.h>

namespace HomeBitmapAssets {
constexpr int kStatusIconW = 16;
constexpr int kStatusIconH = 16;
constexpr int kHomeRowIconW = 20;
constexpr int kHomeRowIconH = 20;
constexpr int kWeatherIconW = 20;
constexpr int kWeatherIconH = 20;
extern const uint8_t kWifi[];
extern const uint8_t kNoWifi[];
extern const uint8_t kBattery25[];
extern const uint8_t kBattery50[];
extern const uint8_t kBattery75[];
extern const uint8_t kBatteryFull[];
extern const uint8_t kWeatherPlaceholder[];
extern const uint8_t kPomodoroClock[];
extern const uint8_t kTodo[];
const uint8_t* weatherIcon(uint16_t code);
}
