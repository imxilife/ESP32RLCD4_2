#pragma once

#include <Arduino.h>

/**
 * 时间结构体
 */
struct RTCTime {
    uint16_t year;    // 年份（2020-2119）
    uint8_t month;    // 月份（1-12）
    uint8_t day;      // 日期（1-31）
    uint8_t hour;     // 小时（0-23）
    uint8_t minute;   // 分钟（0-59）
    uint8_t second;   // 秒（0-59）
    uint8_t weekday;  // 星期（0=周日, 1=周一...6=周六）
    
    // 转换为 Unix 时间戳（秒，从 2020-01-01 00:00:00 开始）
    uint32_t toTimestamp() const;
    
    // 从时间戳创建（基准：2020-01-01 00:00:00）
    static RTCTime fromTimestamp(uint32_t timestamp);
};

/**
 * 闹钟回调函数类型
 */
typedef void (*AlarmCallback)();

/**
 * PCF85063 RTC 驱动
 * 注意：PCF85063 与 PCF8563 寄存器布局不同
 * - 时间寄存器起始地址：0x04（PCF8563 是 0x02）
 * - 年份基准：2020（PCF8563 是 2000）
 */
class RTC85063 {
public:
    /**
     * 初始化 RTC
     * @param sda SDA 引脚
     * @param scl SCL 引脚
     */
    void begin(int sda, int scl);

    /**
     * 设置时间
     * @param year 年份（2020-2119）
     * @param month 月份（1-12）
     * @param day 日期（1-31）
     * @param hour 小时（0-23）
     * @param minute 分钟（0-59）
     * @param second 秒（0-59）
     * @param weekday 星期（0=周日, 1=周一...6=周六）
     */
    void setTime(uint16_t year, uint8_t month, uint8_t day, 
                 uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday);

    /**
     * 读取当前时间
     * @return RTCTime 结构体
     */
    RTCTime now();

    /**
     * 设置闹钟（相对当前时间）
     * @param secondsFromNow 从当前时间起多少秒后触发
     * @param durationSeconds 闹钟持续时间（秒）
     * @param callback 闹钟触发时的回调函数
     */
    void setAlarm(uint32_t secondsFromNow, uint32_t durationSeconds, AlarmCallback callback);

    /**
     * 取消闹钟
     */
    void cancelAlarm();

    /**
     * 闹钟监听器（需要在 loop 中定期调用）
     */
    void alarmListener();

private:
    static const uint8_t PCF85063_ADDR = 0x51;
    
    // 闹钟状态（使用时间戳）
    bool _alarmEnabled;
    bool _alarmTriggered;
    uint32_t _alarmTime;        // 闹钟触发时间戳
    uint32_t _alarmOffTime;     // 闹钟关闭时间戳
    AlarmCallback _alarmCallback;
    uint32_t _lastCheckMs;
    
    // BCD 编码/解码
    static uint8_t bin2bcd(uint8_t val);
    static uint8_t bcd2bin(uint8_t val);
};
