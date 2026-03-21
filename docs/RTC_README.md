# PCF85063 RTC 驱动说明

## 问题解决记录

### 根本原因
项目使用的 RTC 芯片是 **PCF85063 ATL**，而不是 PCF8563。这两个芯片虽然都是 NXP 的 RTC，但有关键差异：

| 差异点 | PCF8563 | PCF85063 |
|--------|---------|----------|
| **时间寄存器起始地址** | 0x02 | 0x04 |
| **年份基准** | 2000 | 2020 |
| **寄存器布局** | 0x02=秒, 0x03=分, 0x04=时... | 0x04=秒, 0x05=分, 0x06=时... |

### 症状
- 使用 Adafruit RTClib 的 `RTC_PCF8563` 类时，时间显示错误
- "小时"字段每秒递增（实际读到的是秒寄存器）
- 年份显示 2006 而不是 2026（基准年不同）

### 解决方案
完全绕过 RTClib，直接按 PCF85063 数据手册读写寄存器：
- 写入时间：从 0x04 开始写 7 个寄存器
- 读取时间：从 0x04 开始读 7 个寄存器并手动 BCD 解码
- 年份基准改为 2020（存 06 表示 2026）

## 模块化设计

### 文件结构
```
src/
├── RTC85063.h       # PCF85063 驱动头文件
├── RTC85063.cpp     # PCF85063 驱动实现
└── main.cpp         # 主程序（调用 RTC 驱动）
```

### API 说明

#### 初始化
```cpp
void begin(int sda, int scl);
```
初始化 I2C 总线并准备 RTC 驱动。

#### 时间设置
```cpp
void setTime(uint16_t year, uint8_t month, uint8_t day, 
             uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday);
```
- `year`: 完整年份（2020-2119）
- `month`: 月份（1-12）
- `day`: 日期（1-31）
- `hour`: 小时（0-23）
- `minute`: 分钟（0-59）
- `second`: 秒（0-59）
- `weekday`: 星期（0=周日, 1=周一, ..., 6=周六）

#### 时间读取
```cpp
RTCTime getTime();
DateTime now();
```
- `getTime()`: 返回 `RTCTime` 结构体，包含完整时间信息
- `now()`: 返回 `DateTime` 对象（兼容 Adafruit RTClib）

#### 闹钟设置
```cpp
void setAlarm(uint32_t secondsFromNow, uint32_t durationSeconds, AlarmCallback callback);
```
- `secondsFromNow`: 从当前时间起多少秒后触发
- `durationSeconds`: 闹钟持续时间（秒）
- `callback`: 闹钟触发时的回调函数

#### 闹钟取消
```cpp
void cancelAlarm();
```
取消当前设置的闹钟。

#### 闹钟监听
```cpp
void alarmListener();
```
检查闹钟状态，必须在 `loop()` 中定期调用。内部已实现节流（每秒检查一次）。

### 使用方法

#### 基本时间操作

```cpp
#include "RTC85063.h"

// 创建 RTC 实例
RTC85063 rtc;

void setup() {
    // 初始化 I2C（SDA=GPIO13, SCL=GPIO14）
    rtc.begin(13, 14);
    
    // 设置时间：2026/03/02 17:50:00 星期一
    rtc.setTime(2026, 3, 2, 17, 50, 0, 1);  // 1=星期一
    
    // 读取当前时间（方式1：使用 RTCTime 结构体）
    RTCTime time = rtc.getTime();
    Serial.printf("%04d/%02d/%02d %02d:%02d:%02d (weekday=%d)\n",
                  time.year, time.month, time.day,
                  time.hour, time.minute, time.second, time.weekday);
    
    // 读取当前时间（方式2：使用 DateTime 对象，兼容旧接口）
    DateTime now = rtc.now();
}

void loop() {
    // 定期读取时间
    RTCTime time = rtc.getTime();
}
```

#### 闹钟功能

```cpp
#include "RTC85063.h"

RTC85063 rtc;

// 闹钟回调函数
void onAlarmTriggered() {
    Serial.println("Alarm triggered!");
    // 在这里执行闹钟触发时的操作
}

void setup() {
    rtc.begin(13, 14);
    rtc.setTime(2026, 3, 2, 17, 50, 0, 1);
    
    // 设置闹钟：10秒后触发，持续3秒
    rtc.setAlarm(10, 3, onAlarmTriggered);
}

void loop() {
    // 必须定期调用闹钟监听器
    rtc.alarmListener();
}
```

#### 取消闹钟

```cpp
// 取消当前设置的闹钟
rtc.cancelAlarm();
```

## PCF85063 寄存器映射

| 地址 | 寄存器名 | 说明 |
|------|----------|------|
| 0x04 | Seconds | 秒（BCD 格式，0-59） |
| 0x05 | Minutes | 分（BCD 格式，0-59） |
| 0x06 | Hours | 时（BCD 格式，0-23） |
| 0x07 | Days | 日（BCD 格式，1-31） |
| 0x08 | Weekdays | 星期（0-6） |
| 0x09 | Months | 月（BCD 格式，1-12） |
| 0x0A | Years | 年（BCD 格式，0-99，基准 2020） |

## 硬件连接

- **I2C 地址**: 0x51（7-bit）
- **SDA**: GPIO13
- **SCL**: GPIO14
- **晶振**: 32.768 kHz

## 注意事项

1. **不要使用 Adafruit RTClib 的 `RTC_PCF8563` 类**，它与 PCF85063 不兼容
2. **年份范围**: 2020-2119（存储 0-99）
3. **BCD 格式**: 所有时间字段都使用 BCD 编码（如 26 = 0x26）
4. **I2C 速率**: 支持最高 400 kHz（Fast-mode）

## 参考资料

- [PCF85063A 数据手册](https://www.uugear.com/doc/datasheet/PCF85063A.pdf)
- [NXP PCF85063ATL 产品页](https://nxp.com/part/PCF85063ATL)
