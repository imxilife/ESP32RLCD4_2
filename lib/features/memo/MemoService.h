#pragma once

#include <Arduino.h>
#include <core/app_message/app_message.h>
#include <device/rtc/RTC85063.h>

struct MemoItem {
    uint16_t id = 0;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t priority = 0;
    bool active = false;
    char text[MEMO_TEXT_MAX] = {};
};

namespace MemoService {
void begin();
uint16_t addMemo(const RTCTime& date, const char* text, uint8_t priority);
bool deleteMemo(uint16_t id);
uint8_t topToday(const RTCTime& date, MemoItem* out, uint8_t maxCount);
uint8_t countToday(const RTCTime& date);
void postUpdate(const RTCTime& date);
}
