# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash

```bash
# Build
pio run -e esp32s3box

# Build & upload (115200 baud to reduce "No serial data received" errors)
pio run -e esp32s3box -t upload

# Serial monitor
pio device monitor

# Build + upload + monitor in one line
pio run -e esp32s3box -t upload && pio device monitor
```

## GUI Tests

`ENABLE_GUI_TESTS` 宏（默认 `0`）控制 `setup()` 末尾是否运行 `GuiTests::runAllTests(gui)`，用于验证绘图原语和字体渲染。置 `1` 后烧录即可在实机上执行。

## Font Generation

```bash
# Interactive CLI — requires Pillow (pip install Pillow)
python tools/gen_font_digits.py
```

The tool prompts for: mode (ASCII / 中文), TTF/OTF file from the project root, target glyph size (e.g. `18x18`), and character set. It renders via 4× supersampling + LANCZOS resize, shows a terminal preview, then writes `.cpp` + `.h` to `lib/gui/fonts/`. Place TTF/OTF files directly in the project root so the tool can discover them.

Generated file naming: `Font_{ascii|chinese}_{FontStem}_{W}_{H}.{cpp,h}`, e.g. `Font_ascii_Geom_Bold_20_20.h`.

## Architecture

### Hardware
ESP32-S3-BOX running FreeRTOS. Display is 400×300 monochrome reflective LCD (1bpp) driven over SPI by `DisplayPort` in [lib/display/display_bsp.h](lib/display/display_bsp.h). Pixel index lookup uses a precomputed LUT (`AlgorithmOptimization = 3`). I²C bus: RTC PCF85063 at 0x51 (SDA=GPIO13, SCL=GPIO14); SHTC3 humidity sensor shares the same bus.

按键：KEY1=GPIO18，KEY2=GPIO0（均为低电平有效，内部上拉）。

### Directory layout
`src/` contains only the entry point. All reusable code lives in `lib/`:

```
src/
  main.cpp              ← 全局实例、ISR、setup/loop（无业务逻辑）
lib/
  app_message/          ← AppMessage 结构体、MsgType 枚举（跨库共享）
  app_tasks/            ← FreeRTOS 任务：rtcTask、humitureTask、wifiTask、batteryTask
  input_key/            ← InputKeyManager（中断+定时器去抖）、KeyEvent
  state_manager/        ← StateManager、AbstractState、StateId
  state/                ← MainUIState、PomodoroState、MusicPlayerState、XZAIState
  pomodoro/             ← Pomodoro 计时逻辑（纯状态机，无 GPIO）
  ui_clock/             ← 时钟/日期绘制：handleRtcUpdate、drawTime、drawDateWeek
  gui/                  ← Gui 绘图 API、Font 系统、fonts/ 字模文件
  display/              ← DisplayPort：SPI 初始化、帧缓冲、RLCD_Display()
  rtc/                  ← PCF85063 驱动
  humiture/             ← SHTC3 温湿度传感器封装
  wifi_manager/         ← WiFiConfig（WiFiManager 封装 + NTP 同步）
  gui_tests/            ← GuiTests：绘图原语和字体渲染测试
```

### 整体数据流

```
FreeRTOS Tasks (rtc/humiture/wifi/battery)
        │  xQueueSend
        ▼
   g_msgQueue  ◄── GPIO ISR (touch, 预留)
        │  xQueueReceive（loop，50ms 超时）
        ▼
 stateManager.dispatch(msg)
        │
        ▼
 当前 State.onMessage()   →  gui_ 绘图（帧缓冲）
                                   │
                              gui.display()  ← loop() 末尾唯一刷屏点

GPIO ISR (KEY1/KEY2 CHANGE — FALLING+RISING)
        │
        ▼
 InputKeyManager（去抖状态机，FreeRTOS 软件定时器）
        │  fireEvent()
        ▼
 stateManager.dispatchKeyEvent(event)
        │
        ▼
 当前 State.onKeyEvent()   →  requestTransition() 延迟切换
```

### 状态机（StateManager + AbstractState）

- **StateManager** (`lib/state_manager/`)：注册子状态、维持当前状态、分发消息和按键事件；在 `dispatch()` / `dispatchKeyEvent()` 末尾执行延迟状态切换（`doTransition()`）。
- **AbstractState** (`lib/state_manager/AbstractState.h`)：四个纯虚接口 `onEnter/onExit/onMessage/onKeyEvent`；protected `requestTransition(StateId)` 请求切换。
- **StateId** (`lib/state_manager/StateId.h`)：`MAIN_UI=0, POMODORO=1, MUSIC_PLAYER=2, XZAI=3`。

状态切换循环（KEY1 驱动）：
```
MAIN_UI → POMODORO → MUSIC_PLAYER → XZAI → MAIN_UI
```
番茄时钟 EXIT 事件 → `MUSIC_PLAYER`；其余状态 KEY1 DOWN → 下一状态。

### 按键子系统（InputKeyManager）

仿 Android InputSubSystem，中断 + FreeRTOS 软件定时器去抖：

```
IDLE
  → FALLING ISR → 启动 5ms 去抖定时器 → DEBOUNCE
DEBOUNCE
  → 到期，GPIO LOW  → DOWN + 启动 500ms 长按定时器 → LONG_PRESS
  → 到期，GPIO HIGH → IDLE（噪声）
LONG_PRESS
  → RISING ISR → UP_PENDING（1ms 定时器立即调度 UP）
  → 定时器到期，GPIO LOW  → LONG_PRESS 事件 + 启动 120ms 重复定时器 → REPEAT
  → 定时器到期，GPIO HIGH → UP → IDLE（兜底短按）
UP_PENDING
  → 1ms 定时器到期 → UP → IDLE
REPEAT
  → RISING ISR → UP_PENDING（1ms 定时器立即调度 UP）
  → 定时器到期，GPIO LOW  → LONG_REPEAT（定时器自动重载）
  → 定时器到期，GPIO HIGH → UP → IDLE（兜底）
```

中断模式：`CHANGE`（同时捕获 FALLING 和 RISING），按键释放立即感知，消除 LONG_PRESS 窗口期内丢失按键的问题。

### 各状态职责

| State | onEnter | onKeyEvent |
|---|---|---|
| `MainUIState` | 首次进入启动 4 个后台任务（once-flag）；清屏；resetClockState | KEY1 DOWN → POMODORO |
| `PomodoroState` | `pomodoro_.enterSetup()` | KEY1 → `onKey1()`；KEY2 → `onKey2Short()` |
| `MusicPlayerState` | 清屏显示占位文字 | KEY1 DOWN → XZAI |
| `XZAIState` | 清屏显示占位文字 | KEY1 DOWN → MAIN_UI |

`MainUIState` 还负责：
- 注册 WiFiConfig 的静态回调（`registerCallbacks(wifiConfig)`，在 `setup()` 调用）
- `MSG_NTP_SYNC` → `rtc_.setTime(...)` 写入 RTC，再切回时钟显示

### FreeRTOS message queue pattern
`g_msgQueue`（`main.cpp` 定义，`extern` 访问）是唯一数据通道：后台任务和 ISR 发送 `AppMessage`，`loop()` 是唯一消费者，`gui.display()` 是唯一刷屏点。消息类型定义在 [lib/app_message/app_message.h](lib/app_message/app_message.h)。

### Font system
`Font` struct ([lib/gui/Font.h](lib/gui/Font.h)) carries:
- `lineHeight` — Y advance between lines
- `getGlyph(codepoint, &w, &h, &strideBytes, &advanceX, data)` — returns 1bpp bitmap or `nullptr`
- `fallback` — next font to try when a glyph is missing

All glyph bitmaps are **1bpp row-major, MSB-left**: `bit = data[row * stride + (col >> 3)] >> (7 - (col & 7)) & 1`.

`Gui::drawText` → `drawTextImpl` decodes UTF-8, walks the fallback chain per codepoint, and applies `GlyphEffect_Apply` for bold/outline before blitting.

Currently active fonts:
| Variable | File | Use |
|---|---|---|
| `kFont_Alibaba72x96` | `Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_72_96` | Large clock digits |
| `kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18` | `Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_12_18` | Date (MM/DD) |
| `kFont18_AlibabaPuHuiTi_3_75_SemiBold` | `Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18` | Weekday (中文) |

### WiFi
`WiFiConfig` wraps `WiFiManager` (captive-portal provisioning). 静态回调 `MainUIState::wifiUiMessageHandler` / `ntpSyncHandler` 将事件转为队列消息，由 `loop()` 统一处理渲染，不直接操作 `Gui`。

#### WiFi UI 状态机（`wifiUiVisible_` in `MainUIState`）

时钟始终是主界面。WiFi/NTP 状态文案是临时全屏覆盖层：

| 场景 | 清除时机 |
|------|---------|
| NTP 同步成功 | `MSG_NTP_SYNC` → 写入 RTC → 立即清屏显示时间 |
| WiFi 连接失败 | `emitMessage("WiFi连接失败","")` → 延迟 3 秒 → `MSG_WIFI_STATUS` → 清屏 |
| 进入配网门户 | `configModeCallback` 显示 AP URL 4 秒 → `emitMessage("","")` → 识别空消息 → 清屏 |

**空消息约定**：`emitMessage("", "")` 是"清除 WiFi UI 切回时钟"的内部信号。`handleWifiUi` 识别 `line1[0]=='\0' && line2[0]=='\0'` 时执行清屏而非显示。

#### NTP 同步实现注意事项

`syncNTP()` 必须用 `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`（来自 `<esp_sntp.h>`）等待同步完成，**不能**只依赖 `getLocalTime()` 的返回值。

原因：ESP32 Arduino 的 `getLocalTime()` 内部只检查 `tm_year > 116`（年份 > 2016），软重启后系统时钟由 RTC 域保留，`getLocalTime` 会在 SNTP 未完成时立即返回旧时钟值，导致写入 PCF85063 的时间偏差数分钟（内部 RC 振荡器精度差，可达 ±5%）。
