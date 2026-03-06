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

## Enabling Hardware / Running Tests

Everything in `setup()` is gated behind `#define ENABLE_GUI_TESTS` in [src/main.cpp](src/main.cpp). Set it to `1` to activate RTC init, humiture init, queue creation, and RTOS task spawning. Without it, `setup()` only initialises the LCD and `loop()` exits immediately (queue is null).

`GuiTests::runAllTests(gui)` in [lib/gui_tests/GuiTests.cpp](lib/gui_tests/GuiTests.cpp) exercises all drawing primitives and font rendering on the real display.

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

### Directory layout
`src/` contains only the entry point. All reusable code lives in `lib/`:

```
src/
  main.cpp              ← 全局实例、ISR、wifiUiMessageHandler、setup/loop
lib/
  app_message/          ← AppMessage 结构体、MsgType 枚举（跨库共享）
  app_tasks/            ← FreeRTOS 三个任务：rtcTask、humitureTask、wifiTask
  ui_clock/             ← 时钟/日期绘制：handleRtcUpdate、drawTime、drawDateWeek
  gui/                  ← Gui 绘图 API、Font 系统、fonts/ 字模文件
  display/              ← DisplayPort：SPI 初始化、帧缓冲、RLCD_Display()
  rtc/                  ← PCF85063 驱动
  humiture/             ← SHTC3 温湿度传感器封装
  wifi_manager/         ← WiFiConfig（WiFiManager 封装 + NTP 同步）
  gui_tests/            ← GuiTests：绘图原语和字体渲染测试
```

### FreeRTOS message queue pattern
`g_msgQueue` (defined in `main.cpp`, accessed via `extern` in tasks/ISRs) is the single backbone: all three RTOS tasks and both ISRs post `AppMessage` structs to it; `loop()` is the sole consumer and the only place `gui.display()` is called. Message types are defined in [lib/app_message/app_message.h](lib/app_message/app_message.h).

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
| `kFont_ascii_Geom_Bold_20_20` | `Font_ascii_Geom_Bold_20_20` | Date (MM/DD) |
| `kFont18_AlibabaPuHuiTi_3_75_SemiBold` | `Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_18_18` | Weekday (中文) |

### WiFi
`WiFiConfig` wraps `WiFiManager` (captive-portal provisioning). It posts UI text via a `WiFiMessageCallback` instead of touching `Gui` directly — the callback (`wifiUiMessageHandler` in `main.cpp`) converts to queue messages so the main loop handles all rendering.
