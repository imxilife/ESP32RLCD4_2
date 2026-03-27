# Agent Notes

## 用途

本文件给新会话提供 **30 秒协作速览**，重点是：

- 当前仓库是什么
- 先读哪些文件
- 常用命令是什么

它不是完整的项目说明文档。  
如果要理解架构、目录、运行模型，请优先读 [PROJECT_OVERVIEW.md](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/PROJECT_OVERVIEW.md)。

## 项目一句话

`ESP32_RLCD4_2` 是一个运行在 **ESP32-S3-BOX** 上的反射屏 UI 项目，采用 **状态机 + 消息队列 + 统一刷屏出口** 架构。

## 新会话建议顺序

1. [PROJECT_OVERVIEW.md](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/PROJECT_OVERVIEW.md)
2. [src/main.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/src/main.cpp)
3. [lib/core/state_manager/StateManager.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/lib/core/state_manager/StateManager.cpp)
4. [lib/core/state/CarouselState.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/lib/core/state/CarouselState.cpp)

## 当前协作约定

- `gui.display()` 是唯一刷屏出口
- `g_msgQueue` 是主事件通道
- 优先保持模块边界清晰：`core / ui / device / media / features`

## 常用命令

```bash
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio device monitor
```
## 回复要求
* `克制、简洁、直接`
* `修复bug时先定位再修改，避免盲目改动引入新问题`