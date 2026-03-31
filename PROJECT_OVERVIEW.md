# ESP32_RLCD4_2 Project Overview

> 这是本仓库的项目入门主文档。  
> 如果只是新开会话想快速知道先看什么，可先看 `AGENTS.md`；如果要真正理解项目，请从本文件开始。

## 1) 这是什么项目

这是一个基于 **ESP32-S3-BOX + 400x300 单色反射式 LCD** 的嵌入式 UI 项目。  
项目核心是一个事件驱动的状态机系统：后台任务采集数据（RTC、温湿度、Wi-Fi/NTP、电池等），主循环统一分发消息并刷新屏幕。

当前主界面是 Launch 四宫格首页，可进入时钟、番茄钟、音乐等状态。

## 2) 为什么要做它

项目目标是构建一个在低功耗反射屏上可长期运行、可扩展的“信息与交互终端”：

- 验证 ESP32-S3 在多外设并行场景下的稳定性（I2C/SPI/I2S/FreeRTOS）
- 形成可维护的 UI 架构（状态机 + 消息队列 + 统一渲染出口）
- 为后续功能扩展（更多状态、更多媒体能力、联网能力）提供模块化基础

## 3) 技术栈

- **MCU/板卡**: ESP32-S3-BOX
- **框架**: Arduino on ESP-IDF (PlatformIO)
- **并发模型**: FreeRTOS task + queue + timer + ISR
- **图形系统**: 自研 1bpp GUI（Font/Glyph/基本图元）
- **显示**: SPI 驱动反射式 LCD（400x300）
- **传感/外设**: RTC (PCF85063), SHTC3, SD 卡
- **音频**: I2S + ES7210/ES8311，含 MP3 软解码（minimp3）
- **网络**: WiFiManager + NTP（SNTP）
- **构建工具**: PlatformIO (`espressif32`, `board=esp32s3box`)

## 4) 运行架构（高层）

### 数据流

1. 后台任务采集数据并发消息到 `g_msgQueue`
2. `loop()` 从队列取消息并交给 `StateManager::dispatch(...)`
3. 当前状态处理消息/按键并更新 UI 数据
4. 每帧调用 `tickCurrentState()`
5. `gui.display()` 作为唯一刷屏出口

### 关键约束

- `g_msgQueue` 是跨模块事件主通道
- `gui.display()` 只在主循环调用，避免多点刷屏竞争
- 音频链路遵循录放互斥设计，避免声学反馈

## 5) 目录结构与作用（当前）

```text
src/
  main.cpp                    # 启动入口、全局对象、setup/loop、ISR接入

lib/
  core/                       # 核心调度与状态层
    app_message/              # 全局消息定义（MsgType / AppMessage）
    app_tasks/                # 后台任务（rtc/humiture/wifi/battery）
    input_key/                # 按键输入与去抖（中断+定时器）
    state_manager/            # 状态机框架（StateManager/AbstractState/StateId）
    state/                    # 具体状态实现（Carousel/MainUI/Pomodoro/...)

  ui/                         # UI 相关能力
    gui/                      # Gui绘制API、Font系统、字模、字形效果
    animation/                # 动画插值与动画基础
    carousel/                 # 卡片轮播控制器
    view/                     # View 抽象层（CardView/TextView）
    views/
      clock/                  # 时钟页面绘制
      pomodoro/               # 番茄钟页面绘制

  device/                     # 硬件设备访问层
    display/                  # 反射屏驱动
    rtc/                      # RTC驱动
    humiture/                 # 温湿度传感器封装
    sdcard/                   # SD卡读写封装

  media/                      # 媒体处理能力层
    audio/                    # I2S/Codec 录放控制
    mp3_player/               # MP3 解码播放
    bluetooth/                # 蓝牙音频

  features/                   # 业务特性层（可组合 media/device/ui）
    pomodoro/                 # 番茄钟业务逻辑状态机
    network/                  # 配网/NTP业务流程封装

  tests/                      # 设备侧功能测试
    gui/                      # GUI绘制测试
    sdcard/                   # SD卡测试
    audio/                    # 蓝牙音频测试
```

## 6) 常用命令

```bash
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio device monitor
```

## 7) 下次会话建议先读什么

1. 本文件 `PROJECT_OVERVIEW.md`
2. `src/main.cpp`（系统入口与数据流）
3. `lib/core/state_manager/`（状态机机制）
4. `lib/core/state/LaunchState.cpp`（当前主交互）
5. `platformio.ini`（构建与依赖）

## 8) 扩展阅读

- `docs/PSRAM_RAM_GUIDE.md`：ESP32-S3 中 PSRAM / SRAM 区别、适用场景与本项目建议
- `docs/OTA_FLOW_GUIDE.md`：OTA 升级流程、关键 API 与本项目实现入口
- `toosls/`：项目相关工具脚本（如字体生成、字模转换等）
