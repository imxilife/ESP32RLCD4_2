# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash

```bash
pio run -e esp32s3box                                # Build
pio run -e esp32s3box -t upload                      # Build & upload
pio device monitor                                   # Serial monitor
pio run -e esp32s3box -t upload && pio device monitor # Build + upload + monitor
```

## GUI Tests

`ENABLE_GUI_TESTS` 宏（默认 `0`）控制 `setup()` 末尾是否运行 `GuiTests::runAllTests(gui)`。置 `1` 后烧录即可在实机上执行。

`ENABLE_SDCARD_TESTS` 宏（默认 `0`）控制 `setup()` 末尾是否运行 `SDCardTests::runAllTests()`。需插入 SD 卡，置 `1` 后烧录，通过 Serial Monitor 查看测试结果。

## Font Generation

```bash
python tools/gen_font_digits.py   # Interactive CLI, requires Pillow
```

交互式选择 mode (ASCII/中文)、TTF/OTF 字体文件（放项目根目录）、目标字形尺寸、字符集。4x 超采样 + LANCZOS 缩放，输出到 `lib/gui/fonts/`。命名规则：`Font_{ascii|chinese}_{FontStem}_{W}_{H}.{cpp,h}`。

## Architecture

### Hardware

ESP32-S3-BOX running FreeRTOS。

| 外设 | 说明 |
|------|------|
| Display | 400x300 单色反射式 LCD (1bpp), SPI 驱动, `DisplayPort` in [lib/display/display_bsp.h](lib/display/display_bsp.h) |
| I2C Bus | SDA=GPIO13, SCL=GPIO14 |
| RTC | PCF85063 (0x51) |
| 温湿度 | SHTC3 |
| MIC ADC | ES7210 (0x40), MIC1+MIC3, PGA 33dB |
| Speaker DAC | ES8311 (0x18) |
| PA | GPIO46 高电平使能 |
| SD Card | SPI2_HOST (HSPI), MOSI=GPIO21, SCK=GPIO38, MISO=GPIO39, CS=GPIO17 |
| 按键 | KEY1=GPIO18, KEY2=GPIO0（低电平有效，内部上拉） |

### Audio (I2S + Codec)

**I2S 总线（ES7210 和 ES8311 共享）：**

| 信号 | GPIO | 说明 |
|------|------|------|
| MCLK | 16 | 4.096 MHz (ESP32 APLL) |
| BCLK | 9 | 1.024 MHz |
| WS/LRCK | 45 | 16 kHz 采样率 |
| DOUT (TX) | 8 | ESP32 → ES8311 (播放) |
| DIN (RX) | 10 | ES7210 → ESP32 (录音) |

格式：I2S Philips, 16-bit 数据在 32-bit slot 高位, stereo (L=MIC1, R=MIC3)。

**关键设计约束 — 禁止实时回环：**

MIC 和 Speaker 物理距离近，同时工作会形成声学正反馈环路导致啸叫。必须采用**先录后放**模式：
1. 录音阶段：关闭 PA，采集 MIC 到 PSRAM 缓冲区
2. 增益处理：16 倍数字放大 (+24dB)，clamp 到 int16 范围
3. 播放阶段：开启 PA，将缓冲区写入 I2S TX

两阶段物理互斥，从根本上切断反馈环路。API：`AudioCodec::startRecordPlay()` / `stop()`。

**PA 开关时序**：开启时先设 DAC 音量再开 PA；关闭时先关 PA 再 mute DAC（避免 pop 噪声）。操作前调 `i2s_zero_dma_buffer()` 清除 DMA 残留。

**音频调试方法 — 分段隔离验证：**

| 函数 | 验证链路 | 方法 |
|------|---------|------|
| `diagMicInput()` | MIC→ESP32 | PA 关闭，读 I2S RX，打印峰值 |
| `diagSpeakerOutput()` | ESP32→Speaker | 写 1kHz 方波到 I2S TX |
| `diagRecordThenPlay()` | 先录后放（串行） | 录 2s → 播放 |

### MP3 播放器 (Mp3Player)

从 SD 卡 `/music/` 目录读取 .mp3 文件，minimp3 软解码，双缓冲 producer-consumer 架构：

- **Decoder Task** (Pri=5): 读 SD 压缩数据 → mp3dec_decode_frame() → PCM int16 → I2S int32 格式转换 → 填充 ping-pong 缓冲区
- **Writer Task** (Pri=6): 从缓冲区 → i2s_write() → ES8311 DAC → PA → Speaker
- 首帧解码后通过 `i2s_set_clk()` 动态切换采样率（支持 44100/48000 等），播放结束恢复 16kHz
- 与 AudioCodec 共用 I2S_NUM_0，由 MusicPlayerState 保证互斥

### Directory layout

```
src/
  main.cpp              ← 全局实例、ISR、setup/loop（无业务逻辑）
lib/
  app_message/          ← AppMessage 结构体、MsgType 枚举（跨库共享）
  app_tasks/            ← FreeRTOS 任务：rtcTask、humitureTask、wifiTask、batteryTask
  input_key/            ← InputKeyManager（中断+定时器去抖）、KeyEvent
  state_manager/        ← StateManager、AbstractState、StateId
  state/                ← CarouselState、MainUIState、PomodoroState、MusicPlayerState、XZAIState
  view/                 ← View 抽象体系：View 基类、CardView、TextView
  animation/            ← 动画系统：Animation、ScaleAnimation、TranslateAnimation、Interpolator
  carousel/             ← CarouselController、CardDescriptor（旋转木马卡片动画编排）
  pomodoro/             ← Pomodoro 计时逻辑（纯状态机，无 GPIO）
  ui_clock/             ← 时钟/日期绘制：handleRtcUpdate、drawTime、drawDateWeek
  gui/                  ← Gui 绘图 API、Font 系统、fonts/ 字模文件
  display/              ← DisplayPort：SPI 初始化、帧缓冲、RLCD_Display()
  rtc/                  ← PCF85063 驱动
  humiture/             ← SHTC3 温湿度传感器封装
  wifi_manager/         ← WiFiConfig（WiFiManager 封装 + NTP 同步）
  sdcard/               ← SDCard：SD 卡读写封装（SPI2_HOST）
  sdcard_tests/         ← SDCardTests：SD 卡挂载/读写/删除测试
  mp3_player/           ← Mp3Player：MP3 软解码播放（minimp3 + 双缓冲）
  gui_tests/            ← GuiTests：绘图原语和字体渲染测试
```

### 数据流

```
FreeRTOS Tasks (rtc/humiture/wifi/battery)
        │  xQueueSend
        ▼
   g_msgQueue  ◄── GPIO ISR
        │  xQueueReceive（loop，50ms 超时）
        ▼
 stateManager.dispatch(msg) → 当前 State.onMessage() → gui 绘图
        │
 stateManager.tickCurrentState() → 当前 State.tick()（驱动动画等）
        │
   gui.display() ← loop() 末尾唯一刷屏点

GPIO ISR (KEY1/KEY2 CHANGE)
        │
 InputKeyManager（去抖状态机）→ stateManager.dispatchKeyEvent() → 当前 State.onKeyEvent()
```

`g_msgQueue` 是唯一数据通道。消息类型定义在 [lib/app_message/app_message.h](lib/app_message/app_message.h)。

### 状态机

- **StateManager** (`lib/state_manager/`)：注册子状态、维持当前状态、分发消息和按键事件；`dispatch()` / `dispatchKeyEvent()` 末尾执行延迟状态切换；`tickCurrentState()` 每帧调用当前状态的 `tick()`。
- **AbstractState**：纯虚接口 `onEnter/onExit/onMessage/onKeyEvent`；`tick()` 虚方法（默认空实现）；protected `requestTransition(StateId)`。
- **StateId**：`CAROUSEL=0, MAIN_UI=1, POMODORO=2, MUSIC_PLAYER=3, XZAI=4, BLUETOOTH=5`。

**初始状态为 `CarouselState`**（旋转木马主界面），管理所有功能卡片。KEY1 切换卡片并播放缩放+平移动画。

| State | 说明 |
|---|---|
| `CarouselState` | **初始状态**。显示卡片列表（时间/Pomodoro/MusicPlay/XZAI），KEY1 循环切换卡片（500ms 动画），时间卡片动态显示 RTC 时间 |
| `MainUIState` | 时钟界面（保留，当前不作为初始状态） |
| `PomodoroState` | 番茄钟（保留） |
| `MusicPlayerState` | 音乐播放（保留） |
| `XZAIState` | XZAI（保留） |

### 旋转木马动画系统

**View 体系** (`lib/view/`)：Android 风格视图抽象。
- `View`：基类，包含位置/尺寸/颜色/可见性，按键事件 DOWN→UP 转化为 onClick 回调。
- `CardView`：圆角矩形卡片，支持 `drawScaled(gui, cx, cy, scale)` 缩放绘制。
- `TextView`：文本视图。

**动画系统** (`lib/animation/`)：全部 header-only。
- `Animation`：基类，`start()/tick(deltaMs)/isFinished()`，返回插值进度 [0,1]。
- `Interpolator`：函数指针类型，内置 `kLinear` 和 `kEaseInOut`（三次缓动）。
- `ScaleAnimation` / `TranslateAnimation`：缩放和平移动画值计算。
- `AnimationSet`：并行组合缩放+平移。

**CarouselController** (`lib/carousel/`)：编排多卡片布局和动画。
- 卡片沿水平方向排列在"槽"中，当前卡片居中 scale=1.0，相邻卡片 scale=0.5。
- 动画时所有槽平滑左移一个槽位（EaseInOut 插值，500ms）。
- 完全在屏幕外的卡片跳过绘制（裁剪优化）。
- `drawStatic()` 绘制静态布局，`tick()` 推进动画并绘制。

### 按键子系统（InputKeyManager）

仿 Android InputSubSystem，中断 + FreeRTOS 软件定时器去抖。中断模式 `CHANGE`（同时捕获 FALLING 和 RISING）。

状态转换：`IDLE → DEBOUNCE(5ms) → LONG_PRESS(500ms) → REPEAT(120ms)`，RISING ISR 触发 `UP_PENDING → UP → IDLE`。

### Font system

`Font` struct ([lib/gui/Font.h](lib/gui/Font.h))：`lineHeight`、`getGlyph()`、`fallback` 链。字形位图 1bpp row-major, MSB-left。`Gui::drawText` → `drawTextImpl` 解码 UTF-8，遍历 fallback 链，应用 `GlyphEffect_Apply`。

| Variable | Use |
|---|---|
| `kFont_Alibaba72x96` | 大号时钟数字 |
| `kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18` | 日期 (MM/DD) |
| `kFont18_AlibabaPuHuiTi_3_75_SemiBold` | 星期 (中文) |

### WiFi

`WiFiConfig` 封装 `WiFiManager`（captive-portal 配网）。静态回调将事件转为队列消息，由 `loop()` 统一处理。

WiFi UI 是临时全屏覆盖层，清除后回到时钟。空消息 `emitMessage("","")` 是"清除 WiFi UI"的内部信号。

**NTP 注意**：`syncNTP()` 必须用 `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED` 等待同步完成，不能只依赖 `getLocalTime()`（软重启后会返回旧 RTC 值）。
