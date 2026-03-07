#include "RTC85063.h"

// 每月天数（非闰年）
static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// 判断是否为闰年
static bool isLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// RTCTime 转时间戳（基准：2020-01-01 00:00:00）
uint32_t RTCTime::toTimestamp() const {
    uint32_t timestamp = 0;
    
    // 累加年份的秒数
    for (uint16_t y = 2020; y < year; y++) {
        timestamp += isLeapYear(y) ? 366 * 86400UL : 365 * 86400UL;
    }
    
    // 累加月份的秒数
    for (uint8_t m = 1; m < month; m++) {
        uint8_t days = daysInMonth[m - 1];
        if (m == 2 && isLeapYear(year)) {
            days = 29;
        }
        timestamp += days * 86400UL;
    }
    
    // 累加天、时、分、秒
    timestamp += (day - 1) * 86400UL;
    timestamp += hour * 3600UL;
    timestamp += minute * 60UL;
    timestamp += second;
    
    return timestamp;
}

// 时间戳转 RTCTime（基准：2020-01-01 00:00:00）
RTCTime RTCTime::fromTimestamp(uint32_t timestamp) {
    RTCTime time;
    
    // 计算年份
    time.year = 2020;
    while (true) {
        uint32_t yearSeconds = isLeapYear(time.year) ? 366 * 86400UL : 365 * 86400UL;
        if (timestamp < yearSeconds) break;
        timestamp -= yearSeconds;
        time.year++;
    }
    
    // 计算月份
    time.month = 1;
    while (time.month <= 12) {
        uint8_t days = daysInMonth[time.month - 1];
        if (time.month == 2 && isLeapYear(time.year)) {
            days = 29;
        }
        uint32_t monthSeconds = days * 86400UL;
        if (timestamp < monthSeconds) break;
        timestamp -= monthSeconds;
        time.month++;
    }
    
    // 计算日、时、分、秒
    time.day = timestamp / 86400UL + 1;
    timestamp %= 86400UL;
    time.hour = timestamp / 3600UL;
    timestamp %= 3600UL;
    time.minute = timestamp / 60UL;
    time.second = timestamp % 60UL;
    
    // 计算星期（2020-01-01 是星期三）
    uint32_t totalDays = (time.year - 2020) * 365;
    for (uint16_t y = 2020; y < time.year; y++) {
        if (isLeapYear(y)) totalDays++;
    }
    for (uint8_t m = 1; m < time.month; m++) {
        totalDays += daysInMonth[m - 1];
        if (m == 2 && isLeapYear(time.year)) totalDays++;
    }
    totalDays += time.day - 1;
    time.weekday = (totalDays + 3) % 7;  // 2020-01-01 是星期三(3)
    
    return time;
}

void RTC85063::begin(int sda, int scl) {
    Wire.begin(sda, scl);
    
    // 初始化闹钟状态
    _alarmEnabled = false;
    _alarmTriggered = false;
    _alarmCallback = nullptr;
    _lastCheckMs = 0;
}

void RTC85063::setTime(uint16_t year, uint8_t month, uint8_t day, 
                       uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday) {
    // PCF85063 时间寄存器从 0x04 开始
    // 0x04=Seconds, 0x05=Minutes, 0x06=Hours, 0x07=Days, 0x08=Weekdays, 0x09=Months, 0x0A=Years
    
    // 年份转换：2020-2119 -> 0-99
    uint8_t yearOffset = (year >= 2020) ? (year - 2020) : 0;
    
    uint8_t buf[7];
    buf[0] = bin2bcd(second) & 0x7F;   // 0x04: Seconds
    buf[1] = bin2bcd(minute) & 0x7F;   // 0x05: Minutes
    buf[2] = bin2bcd(hour) & 0x3F;     // 0x06: Hours
    buf[3] = bin2bcd(day) & 0x3F;      // 0x07: Days
    buf[4] = weekday & 0x07;           // 0x08: Weekdays
    buf[5] = bin2bcd(month) & 0x1F;    // 0x09: Months
    buf[6] = bin2bcd(yearOffset);      // 0x0A: Years
    
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(0x04);  // 从 Seconds 寄存器开始
    for (int i = 0; i < 7; i++) {
        Wire.write(buf[i]);
    }
    Wire.endTransmission();
    
    Serial.printf("RTC time set: %04d/%02d/%02d %02d:%02d:%02d (weekday=%d)\n",
                  year, month, day, hour, minute, second, weekday);
}

RTCTime RTC85063::now() {
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(0x04);
    Wire.endTransmission();
    
    uint8_t buf[7];
    Wire.requestFrom(PCF85063_ADDR, 7);
    for (int i = 0; i < 7; i++) {
        buf[i] = Wire.available() ? Wire.read() : 0;
    }
    
    RTCTime time;
    time.second = bcd2bin(buf[0] & 0x7F);
    time.minute = bcd2bin(buf[1] & 0x7F);
    time.hour = bcd2bin(buf[2] & 0x3F);
    time.day = bcd2bin(buf[3] & 0x3F);
    time.weekday = buf[4] & 0x07;
    time.month = bcd2bin(buf[5] & 0x1F);
    time.year = 2020 + bcd2bin(buf[6]);

#if DISABLED
    Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n",
                  time.year, time.month, time.day, time.hour, time.minute, time.second);
    Serial.println();
#endif
    return time;
}

void RTC85063::setAlarm(uint32_t secondsFromNow, uint32_t durationSeconds, AlarmCallback callback) {
    RTCTime now = this->now();
    uint32_t currentTimestamp = now.toTimestamp();
    
    _alarmTime = currentTimestamp + secondsFromNow;
    _alarmOffTime = _alarmTime + durationSeconds;
    _alarmCallback = callback;
    _alarmEnabled = true;
    _alarmTriggered = false;
    
    Serial.printf("Alarm set: trigger in %d seconds, duration %d seconds\n", 
                  secondsFromNow, durationSeconds);
}

void RTC85063::cancelAlarm() {
    _alarmEnabled = false;
    _alarmTriggered = false;
    _alarmCallback = nullptr;
    
    Serial.println("Alarm cancelled");
}

void RTC85063::alarmListener() {
    if (!_alarmEnabled) {
        return;
    }
    
    // 节流：每秒检查一次
    uint32_t nowMs = millis();
    if (nowMs - _lastCheckMs < 1000) {
        return;
    }
    _lastCheckMs = nowMs;
    
    RTCTime now = this->now();
    uint32_t currentTimestamp = now.toTimestamp();
    
    // 检查闹钟触发
    if (!_alarmTriggered && currentTimestamp >= _alarmTime) {
        _alarmTriggered = true;
        Serial.println("ALARM TRIGGERED");
        
        // 调用回调函数
        if (_alarmCallback != nullptr) {
            _alarmCallback();
        }
    }
    
    // 检查闹钟关闭
    if (_alarmTriggered && currentTimestamp >= _alarmOffTime) {
        _alarmTriggered = false;
        _alarmEnabled = false;
        Serial.println("ALARM OFF");
    }
}

uint8_t RTC85063::bin2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

uint8_t RTC85063::bcd2bin(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}
