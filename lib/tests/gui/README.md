# GUI 测试模块说明

## 概述
本目录包含 GUI 图形库的测试用例，用于验证各种绘图功能。

## 测试文件

### GuiTests.h / GuiTests.cpp
包含以下测试函数：

#### 1. testBasicShapes() - 基本几何图形测试
- 矩形边框 (`drawRect`)
- 实心矩形 (`fillRect`)
- 圆形边框 (`drawCircle`)
- 实心圆 (`fillCircle`)

#### 2. testRoundRect() - 圆角矩形测试
- 圆角矩形边框 (`drawRoundRect`)
- 实心圆角矩形 (`fillRoundRect`)

#### 3. testTriangleAndLines() - 三角形和线段测试
- 三角形边框 (`drawTriangle`)
- 实心三角形 (`fillTriangle`)
- 对角线、水平线、垂直线 (`drawLine`)

#### 4. testAsciiText() - ASCII 文本测试
- 仅前景色文本（透明背景）
- 前景/背景色文本（覆盖背景）
- 显式指定颜色的文本

#### 5. testUTF8Text() - UTF-8 文本测试
- UTF-8 字符串渲染（含中文）
- 不同颜色组合
- 大号数字字体效果

#### 6. runAllTests() - 运行所有测试
依次执行上述所有测试用例。

## 使用方法

### 在 main.cpp 中启用测试

修改 `main.cpp` 中的宏定义：

```cpp
#define ENABLE_GUI_TESTS 1  // 启用测试
```

### 在 setup() 中调用测试

```cpp
#if ENABLE_GUI_TESTS
    GuiTests::runAllTests(gui);
    delay(5000);  // 显示 5 秒后继续
    gui.clear();
#endif
```

### 运行单个测试

```cpp
#if ENABLE_GUI_TESTS
    gui.clear();
    GuiTests::testBasicShapes(gui);
    gui.display();
    delay(3000);
#endif
```

## 测试流程

1. 启用 `ENABLE_GUI_TESTS` 宏
2. 编译并上传固件
3. 观察 LCD 屏幕上的图形显示
4. 验证各项功能是否正常
5. 测试完成后，将宏改回 `0` 恢复正常功能

## 注意事项

- 测试代码会占用屏幕显示，建议测试完成后禁用
- 每个测试函数可以单独调用，便于调试特定功能
- 测试时建议添加适当的延时，以便观察显示效果
- 生产环境请确保 `ENABLE_GUI_TESTS` 设置为 `0`
