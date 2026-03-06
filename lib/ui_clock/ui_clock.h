#pragma once
#include <RTC85063.h>

// 绘制时间（大号 72x96）
void drawTime(const RTCTime &t);

// 绘制日期和星期（右上角小号）
void drawDateWeek(const RTCTime &t);

// MSG_RTC_UPDATE 分发处理：按分钟/按天触发重绘
void handleRtcUpdate(const RTCTime &t);

// 重置上次绘制缓存，使下次 handleRtcUpdate 强制全量重绘（NTP 同步后调用）
void resetClockState();
