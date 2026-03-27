# Session Brief (30s)

## 项目一句话

`ESP32_RLCD4_2` 是一个运行在 **ESP32-S3-BOX** 上的反射屏 UI 系统，采用 **状态机 + 消息队列** 架构，支持时钟/番茄钟/音乐等功能。

## 当前目标

- 提升 UI 交互体验（Carousel 切换动画、卡片展示）
- 保持代码可维护（模块化目录、清晰依赖边界）
- 为后续扩展（图片、更多联网功能）预留架构空间

## 关键技术栈

- Arduino + FreeRTOS (PlatformIO)
- 400x300 1bpp 反射式 LCD（SPI）
- I2C: RTC + 温湿度；I2S: 音频 Codec
- MP3 软解码（minimp3）
- WiFiManager + NTP

## 关键运行模型

1. 后台任务发送 `AppMessage` 到 `g_msgQueue`
2. `loop()` 取消息并分发到当前状态
3. `tickCurrentState()` 驱动动画与状态逻辑
4. `gui.display()` 是唯一刷屏出口

## 目录速览（重构后）

- `src/main.cpp`：系统入口（setup/loop、全局实例）
- `lib/core/`：状态机与消息系统
- `lib/ui/`：GUI、动画、轮播、页面
- `lib/device/`：硬件驱动（display/rtc/humiture/sdcard）
- `lib/media/`：音频/MP3/蓝牙媒体链路
- `lib/features/`：业务功能（pomodoro/network）
- `lib/tests/`：设备侧测试

## 本地常用命令

```bash
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio device monitor
```

## 下次会话建议先看

1. `如何你想更多了解项目可以看PROJECT_OVERVIEW.md`
2. `src/main.cpp`
3. `lib/core/state_manager/StateManager.cpp`
4. `lib/core/state/CarouselState.cpp`
