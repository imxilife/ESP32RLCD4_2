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
4. [lib/core/state/LaunchState.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/lib/core/state/LaunchState.cpp)

## 当前协作约定

- `gui.display()` 是唯一刷屏出口
- `g_msgQueue` 是主事件通道
- 优先保持模块边界清晰：`core / ui / device / media / features`
- `界面文案只按用户明确要求修改；未被明确指定时，不擅自新增提示词、操作说明或辅助文案`
- `修改界面文案/字体/字号时，必须先检查文本真实像素宽度是否落入容器；不能仅按字符数、固定步进或主观估计直接提交`
- `涉及板级外设排查时，先确认官方示例/BSP实际使用的协议模式和初始化路径，再看当前项目实现；不能只按引脚名主观推断`
- `排查问题时优先拆分阶段，先回答“最前置步骤是否成功”，再继续看后续链路，避免把多个失败点耦合在一起`
- `定位问题时至少同时保留多种思路，例如：协议模式错误、硬件连接错误、官方示例与当前实现不一致；避免单线假设跑太久`
- `修改或新增函数实现时，必须补充必要注释，至少让后续会话能直接看懂函数职责、关键约束和非显然设计点；禁止只靠函数名猜意图`
- 嵌入式开发的特点：`资源有限，需要根据情况权衡性能、内存、代码复杂度`，因此：
  - `克制使用动态内存分配，根据情形使用 RAM 和 PSRAM`
  - `避免过度抽象，保持代码直接和高效`

## 常用命令

```bash
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio device monitor
```
## 回复要求
* `克制、简洁、直接`
* `修复bug时先定位再修改，避免盲目改动引入新问题`
* `给问题解决提供多种可验证思路，并明确当前结论为何成立`
