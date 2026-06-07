# Agent Notes

所有回复使用中文，保持克制、简洁、直接。

## 用途

本文件给新会话提供 30 秒协作速览，重点是：

- 当前仓库是什么
- 先读哪些文件
- 常用命令是什么

它不是完整的项目说明文档。理解架构、目录和运行模型时，优先读 `PROJECT_OVERVIEW.md`。

## 项目一句话

`ESP32_RLCD4_2` 是一个运行在 ESP32-S3-BOX 上的反射屏 UI 项目，采用状态机、消息队列、统一刷屏出口架构。

## 新会话建议顺序

1. `PROJECT_OVERVIEW.md`
2. `src/main.cpp`
3. `lib/core/state_manager/StateManager.cpp`
4. `lib/core/state/LaunchState.cpp`

## 字体资源索引

项目字体统一通过 SPIFFS 中的 bin 文件加载，不再使用脚本生成的全局 C 字模数据。

源字体只保留：

- `fonts/AlibabaPuHuiTi-3-65-m-Medium.otf`：中文主字体，24 号
- `fonts/AlibabaPuHuiTi-3-45-Light.otf`：中文副字体，20 号
- `fonts/NotoSans-Medium.ttf`：英文主字体 18 号、英文副字体 16 号、60 号数字字体

SPIFFS 输出只保留：

- `data/font_zh_main_24.bin`
- `data/font_zh_sub_20.bin`
- `data/font_en_main_18.bin`
- `data/font_en_sub_16.bin`
- `data/font_digits_60.bin`

生成、校验、打包和刷入流程见 `docs/FONT24_BIN_GENERATION.md`。

新增、重命名或修改任何字体 bin 后，必须执行完整 SPIFFS 闭环：先运行 `python tools/gen_project_font_bins.py --target all`，再运行 `pio run -e esp32s3box -t buildfs`，确认输出清单包含目标 `font_*.bin`，最后运行 `pio run -e esp32s3box -t uploadfs` 刷入设备。只执行固件编译或 `upload` 不会更新设备端 SPIFFS，设备仍会加载旧字体文件系统。

历史排查记录见 `docs/TROUBLESHOOTING_CASES.md`。其中 `font24.bin` 和 `FONT_*.bin` 是旧流程记录，当前正式字体文件以上述 6 个 `font_*.bin` 为准。

## 问题排查索引

- `LaunchState` 日志反复出现 `zhMain -> zhSub -> enMain -> enSub` 加载和卸载：优先检查 `FontManagerConfig.maxLoadedFonts` 是否小于当前页面实际使用字体数。当前项目默认值应为 5，覆盖 `zhMain/zhSub/enMain/enSub/digits60`，避免同一页面绘制时 LRU 抖动。
- `LaunchState` 上电一直不显示且日志反复 `load start digits60 /font_digits_60.bin` / `font file missing`：最前置问题是设备端 SPIFFS 没有新生成的 `font_digits_60.bin`，通常是只刷了固件、未执行 `uploadfs`；当前代码已对加载失败字体做单次启动内熔断并让首页时间退回 18 号英文，避免反复卡 SPIFFS。
- `FontTestState` 启动后显示慢：先看 `[BootTrace] first loop display done` 和 `[StateTrace] background services start` 的顺序，后台 WiFi/NTP/Weather 服务必须晚于首帧刷屏；若顺序已正确但仍慢，再看中文行的 `measureMs/drawMs`，优先检查 `measureTextWidth()` 是否只走 metrics、字体 bitmap 是否进入 PSRAM cache。
- `FontTestState` 切入后白屏、字体指针全为空：设备端 SPIFFS 仍是旧文件系统，只包含 `font24.bin` 和旧 `FONT_*.bin`；需要成功执行 `pio run -e esp32s3box -t uploadfs`，并确认串口日志里 6 个 `font_*.bin` 都存在。
- 旧记录：`FONT_16_PHT3_65_M.bin` 的“我”视觉接近 18 号，根因是 16px 中文 bin 生成时只留 1px 边距；当前字体流程见 `tools/gen_project_font_bins.py`。
- 旧记录：`FontBinTestState` 显示 `font24.bin missing`，根因是只刷了固件、未刷 SPIFFS 文件系统分区；当前同类问题仍优先检查 `uploadfs` 是否成功。

## 当前协作约定

- `gui.display()` 是唯一刷屏出口。
- `g_msgQueue` 是主事件通道。
- 优先保持模块边界清晰：`core / ui / device / media / features`。
- 排查类问题解决后，必须把现象、验证链路、根因和修复方式补充到 `docs/TROUBLESHOOTING_CASES.md`，并在本文件问题排查索引里添加或更新记录。
- 界面文案只按用户明确要求修改；未被明确指定时，不擅自新增提示词、操作说明或辅助文案。
- 修改界面文案、字体、字号时，必须先检查文本真实像素宽度是否落入容器；不能仅按字符数、固定步进或主观估计直接提交。
- 生成或修改字体点阵时，中文标点必须单独核对取模效果：逗号、句号、冒号、分号、顿号、问号、感叹号等不能按普通汉字裁剪后拉伸到满格；应保留原字体标点的真实比例、基线和字格内位置。
- 任何新建字体、替换字体源文件、修改字体生成脚本或调整字体输出文件名后，都必须同步更新并刷入 SPIFFS；验证顺序固定为 `gen_project_font_bins.py`、`buildfs` 输出清单、`uploadfs` 成功日志，不能只刷固件后直接看运行现象。
- 涉及板级外设排查时，先确认官方示例/BSP 实际使用的协议模式和初始化路径，再看当前项目实现。
- 排查问题时优先拆分阶段，先回答“最前置步骤是否成功”，再继续看后续链路。
- 定位问题时至少同时保留多种思路，例如协议模式错误、硬件连接错误、官方示例与当前实现不一致。
- 修改或新增函数实现时，必须补充必要注释，至少让后续会话能直接看懂函数职责、关键约束和非显然设计点。
- 嵌入式开发资源有限，需要根据情况权衡性能、内存和代码复杂度；克制使用动态内存分配，按需使用 RAM 和 PSRAM。

## 常用命令

```bash
python tools/gen_project_font_bins.py --target all
pio run -e esp32s3box -t buildfs
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio run -e esp32s3box -t uploadfs
pio device monitor
```

## LVGL 字体转换

https://lvgl.io/tools/fontconverter
