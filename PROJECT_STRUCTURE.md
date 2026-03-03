# ESP32_RLCD4_2 项目结构说明

## 概述
本项目采用模块化设计，将功能代码按模块划分到 `lib` 目录，`src` 目录仅保留主程序入口。

## 目录结构

```
ESP32_RLCD4_2/
├── src/
│   └── main.cpp              # 主程序入口（唯一源文件）
│
├── lib/                      # 功能模块库
│   ├── display/              # 显示驱动模块
│   │   ├── display_bsp.h
│   │   └── display_bsp.cpp
│   │
│   ├── gui/                  # 图形界面模块
│   │   ├── Gui.h             # GUI 主接口
│   │   ├── Gui.cpp
│   │   ├── effects/          # 图形效果子模块
│   │   │   ├── GlyphEffect.h
│   │   │   └── GlyphEffect.cpp
│   │   └── fonts/            # 字体子模块
│   │       ├── Font5x7.h
│   │       ├── Font5x7.cpp
│   │       ├── FontDigits.h
│   │       ├── FontDigits.cpp
│   │       ├── chinese_font.h
│   │       └── chinese_font.cpp
│   │
│   ├── gui_tests/            # GUI 测试模块
│   │   ├── GuiTests.h
│   │   ├── GuiTests.cpp
│   │   └── README.md
│   │
│   ├── rtc/                  # RTC 时钟模块
│   │   ├── RTC85063.h
│   │   └── RTC85063.cpp
│   │
│   └── humiture/             # 温湿度传感器模块
│       ├── Humiture.h
│       └── Humiture.cpp
│
└── platformio.ini            # PlatformIO 配置文件
```

## 模块说明

### 1. display 模块
- **功能**: LCD 显示屏底层驱动
- **主要类**: `DisplayPort`
- **职责**: 
  - SPI 通信初始化
  - 显示缓冲区管理
  - 像素绘制底层接口

### 2. gui 模块
- **功能**: 图形用户界面库
- **主要类**: `Gui`
- **职责**:
  - 基本图形绘制（点、线、矩形、圆等）
  - 文本渲染（ASCII 和 UTF-8）
  - 位图显示
- **子模块**:
  - `effects/`: 图形效果（加粗、描边等）
  - `fonts/`: 字体数据（5x7、数字字体、中文字库）

### 3. rtc 模块
- **功能**: 实时时钟管理
- **主要类**: `RTC85063`
- **职责**:
  - 时间读写
  - 闹钟设置与监听
  - I2C 通信

### 4. humiture 模块
- **功能**: 温湿度传感器封装
- **主要类**: `Humiture`
- **职责**:
  - SHTC3 传感器初始化
  - 温度读取
  - 湿度读取
  - 统一的数据接口

### 5. gui_tests 模块
- **功能**: GUI 功能测试
- **主要类**: `GuiTests`
- **职责**:
  - 基本图形绘制测试
  - 文本渲染测试
  - 提供测试接口

## 使用方式

### 引用模块
在 `main.cpp` 中使用尖括号引用模块头文件：

```cpp
#include <display_bsp.h>  // display 模块
#include <Gui.h>           // gui 模块
#include <RTC85063.h>      // rtc 模块
#include <Humiture.h>      // humiture 模块
#include <GuiTests.h>      // gui_tests 模块（可选）
```

### 启用测试
修改 `main.cpp` 中的宏定义来启用/禁用 GUI 测试：

```cpp
#define ENABLE_GUI_TESTS 1  // 启用测试
#define ENABLE_GUI_TESTS 0  // 禁用测试（生产环境）
```

### 模块内部引用
- 跨模块引用：使用尖括号 `<module.h>`
- 同模块引用：使用引号 `"file.h"` 或相对路径 `"subdir/file.h"`

## 优势

1. **清晰的职责划分**: 每个模块负责特定功能
2. **易于维护**: 模块独立，修改不影响其他部分
3. **可复用性**: 模块可以在其他项目中复用
4. **便于测试**: 可以单独测试每个模块
5. **符合 PlatformIO 规范**: lib 目录下的模块自动被识别和编译

## 编译验证

项目已通过编译验证：
```bash
pio run
```

编译输出显示所有模块正确链接：
- display 模块
- gui 模块（含 effects 和 fonts 子模块）
- rtc 模块
- humiture 模块

## 注意事项

1. `src` 目录仅保留 `main.cpp`，不应添加其他源文件
2. 新增功能应创建新模块或在现有模块中扩展
3. 模块间依赖应保持单向，避免循环依赖
4. 每个模块应有清晰的接口定义（.h 文件）
