#pragma once
#include <RTC85063.h>

// 绘制时间（大号 72x96）
void drawTime(const RTCTime &t);

// 绘制日期和星期（右上角小号）
void drawDateWeek(const RTCTime &t);

// MSG_RTC_UPDATE 分发处理：按分钟/按天触发重绘
void handleRtcUpdate(const RTCTime &t);
