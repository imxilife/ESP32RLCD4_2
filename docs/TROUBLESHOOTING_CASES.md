# 排查问题集

本文件记录已经定位并解决的问题。后续遇到类似排查，优先把结论、验证链路和修复命令补到这里，避免同类问题重复绕路。

注意：本文中的 `font24.bin` 和 `FONT_*.bin` 是旧字体流程的历史排查记录。当前项目字体以 `docs/FONT24_BIN_GENERATION.md` 中的 `font_*.bin` 清单为准。

## 当前字体运行时

当前字体系统入口是 `FontManager::instance().init()`。该调用只负责挂载 SPIFFS、注册字体规格和建立稳定 `Font*` / fallback 链，不会启动时一次性打开字体 bin。

字体首次被 `Gui::drawText()` 或 `Gui::measureTextWidth()` 使用时，`FontManager` 才会调用 `FontBinLoader` 打开对应 bin、校验 `F24BIN1` header，并优先把索引表缓存到 PSRAM；预算允许时也会把 bitmap 区顺序读入 PSRAM，避免后续逐字 SPIFFS 随机读。加载后的字体受 LRU 管理，当前默认最多保留 6 个已加载字体。

如果日志显示 `FontManager init ok` 但首次使用字体时报 `font file missing`，仍优先检查是否执行过 `pio run -e esp32s3box -t uploadfs`。如果报 `bad font magic`、`bad header/index size`、`bad glyph cache metadata`，优先同时检查 `tools/gen_project_font_bins.py` 和 `lib/ui/gui/fonts/FontBinLoader.cpp` 的格式约束。

当前流程图见 `docs/FONT24_BIN_GENERATION.md` 中的 Mermaid 图和 `docs/assets/font-manager-flow.png`。

## 索引

| 日期 | 问题 | 结论 | 相关文件 |
| --- | --- | --- | --- |
| 2026-06-07 | Launch 首页反复 `zhMain/zhSub/enMain/enSub` 加载和卸载 | 当前首页一次绘制会使用 5 类字体；`maxLoadedFonts=3` 使 LRU 在同一页面内持续淘汰刚加载的字体，导致每次刷新都重新从 SPIFFS 读取中文 bitmap。修复为默认保留 6 个项目字体，并在索引和 bitmap 都进入 PSRAM 后关闭文件句柄 | `lib/ui/gui/fonts/FontManager.h`, `lib/ui/gui/fonts/FontRuntime.h/.cpp`, `lib/ui/gui/fonts/FontBinLoader.cpp` |
| 2026-06-07 | Launch 首页上电一直不显示，日志反复加载 `digits72` 失败 | 设备端 SPIFFS 缺少新增的 `/font_digits_72.bin`，只重新编译/刷固件不会更新字体文件系统；同时原逻辑对缺失字体每次绘制都重试，导致反复卡 SPIFFS。修复为字体加载失败熔断、Launch 时间字体缺失时退回 `EnMain`，并仍需执行 `uploadfs` 刷入 6 个字体 bin | `lib/ui/gui/fonts/FontManager.cpp`, `lib/ui/gui/fonts/FontManager.h`, `lib/core/state/LaunchState.cpp`, `data/font_digits_72.bin` |
| 2026-06-06 | `FontTestState` 启动后约 20 秒才显示 | 已实机确认解决。第一阶段卡点是 WiFi/NTP 后台任务早于首帧启动并抢占字体渲染；后台服务延后后仍约 10 秒，剩余卡点收敛到中文字体测宽和绘制：`measureTextWidth()` 误走 bitmap 读取，`drawText()` 对每个字从 SPIFFS 小块随机读；最终修复为首帧后启动后台服务、metrics-only 测宽和 PSRAM bitmap cache | `src/main.cpp`, `lib/core/state_manager/StateManager.cpp`, `lib/core/state/FontTestState.cpp`, `lib/ui/gui/Gui.cpp`, `lib/ui/gui/Font.h`, `lib/ui/gui/fonts/FontManager.cpp`, `lib/ui/gui/fonts/FontBinLoader.cpp`, `lib/ui/gui/fonts/FontRuntime.cpp` |
| 2026-06-04 | `FontTestState` 切入后白屏、字体无法加载 | 设备端 SPIFFS 仍是旧文件系统，只包含 `font24.bin` 和 `FONT_*.bin`，没有当前项目需要的 `font_*.bin`；需要成功执行 `uploadfs` | `src/main.cpp`, `lib/ui/gui/fonts/FontManager.cpp`, `lib/ui/gui/fonts/FontBinLoader.cpp`, `lib/core/state/FontTestState.cpp`, `data/font_*.bin` |
| 2026-06-03 | `FONT_16_PHT3_65_M.bin` 的“我”视觉上接近 18 号 | 16px 中文 bin 生成时只留 1px 边距，实际字形放进 14x14；raw 静态取模是 2px 边距、12x12 可视范围 | `tools/gen_font24_bin.py`, `data/FONT_16_PHT3_65_M.bin`, `lib/ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_16_16_FontTest.cpp`, `lib/core/state/FontBinTestState.cpp` |
| 2026-06-02 | `FontBinTestState` 显示 `font24.bin missing` | 设备未刷入 SPIFFS 文件系统分区，固件能运行但 `/font24.bin` 不在设备端 | `src/main.cpp`, `lib/ui/gui/fonts/Font24Spiffs.cpp`, `lib/core/state/FontBinTestState.cpp`, `data/font24.bin` |

## 2026-06-07：Launch 首页反复加载和卸载字体

### 现象

设备端日志持续重复：

```text
[FontManager] load start zhMain /font_zh_main_24.bin ...
[FontManager] unload zhSub
[FontManager] loaded zhMain ...
[FontManager] load start zhSub /font_zh_sub_20.bin ...
[FontManager] unload enMain
[FontManager] loaded zhSub ...
[FontManager] load start enMain /font_en_main_18.bin ...
[FontManager] unload enSub
```

### 验证链路

`LaunchState` 首页一次绘制会用到多种字体：

- 左栏时间：`digits60`
- 日期和语音状态：`enSub`
- 天气温度：`zhMain`
- 天气状态、番茄状态、备忘录：`zhSub`
- 番茄时间：`enMain`

旧默认 `maxLoadedFonts=3` 小于首页实际字体工作集。每次加载第 4 个字体时，LRU 就会卸载另一个稍后仍会被同一页面用到的字体，形成 `load -> unload -> load` 循环。

当前 6 个字体的索引缓存总量约 126KB，低于 `maxIndexCacheBytes=160KB`；因此抖动的直接原因不是索引预算，而是加载字体数量上限过小。

### 根因

字体缓存策略仍停留在早期“最多 3 个字体常驻”的假设，但首页改为混排中文主字体、中文副字体、英文主字体、英文副字体和首页数字字体后，工作集已超过 3。

### 修复方式

- `FontManagerConfig.maxLoadedFonts` 默认改为 6，覆盖当前全部项目字体槽位。
- `FontRuntime::closeFileIfFullyCached()` 在索引和 bitmap 都进入 PSRAM 后关闭 SPIFFS 文件句柄，避免 6 个字体常驻时长期占用文件句柄。
- LRU 仍保留：如果未来新增更多字体或降低配置，仍会按数量和索引预算淘汰。

复测重点：

```text
[FontManager] init ok maxLoaded=6 ...
```

首页首次绘制可能仍会看到每个字体各加载一次，但稳定后不应再出现同一组 `zhMain/zhSub/enMain/enSub` 在每次刷新中反复 unload/load。

## 2026-06-07：Launch 首页反复加载 digits72 失败后不显示

### 现象

新增 72 号首页数字字体后，重新编译上电，屏幕一直没有正常显示。串口日志反复出现：

```text
[FontManager] load start digits72 /font_digits_72.bin t=41752
[FontManager] load failed /font_digits_72.bin error=font file missing dur=427
```

### 验证链路

本地 `pio run -e esp32s3box -t buildfs` 生成的 SPIFFS 镜像已经包含：

```text
/font_digits_60.bin
/font_en_main_18.bin
/font_en_sub_16.bin
/font_zh_main_24.bin
/font_zh_sub_20.bin
```

设备端日志却显示 `/font_digits_72.bin` 缺失，说明固件已经引用新字体路径，但设备端 SPIFFS 仍是旧文件系统。只执行 `pio run -e esp32s3box` 或 `upload` 不会更新 `data/` 里的字体 bin。

同时，`Gui::measureTextWidth()` 和 `Gui::drawText()` 会沿字体 fallback 链逐字查询。缺失字体没有失败熔断时，每次测宽/绘制都会重新触发 `FontManager::load(Digits72)`，设备上每次缺失检查约 426 ms，导致首页绘制被反复拖住。

### 根因

根因分两层：

- 资源层：新增 `font_digits_72.bin` 后未成功执行 `pio run -e esp32s3box -t uploadfs`，设备端 SPIFFS 没有该文件。
- 代码层：字体加载失败后没有缓存失败状态，导致同一启动周期内对确定缺失的字体反复访问 SPIFFS。

### 修复方式

代码侧：

- `FontManager::FontSlot` 增加 `loadFailed` 熔断标记。某字体首次加载失败后，本次启动内后续 `load()` 直接快速失败，不再重复 `exists/open` 卡 SPIFFS。
- `LaunchState::timeFont()` 先尝试加载 `Digits72`；如果失败，则退回 `EnMain`，保证首页能先显示，而不是被缺失的 72 字体拖住。

设备侧仍必须刷入 SPIFFS：

```bash
python tools/gen_project_font_bins.py --target all
pio run -e esp32s3box -t buildfs
pio run -e esp32s3box -t uploadfs
```

如果同时更新了固件逻辑，还需要执行：

```bash
pio run -e esp32s3box -t upload
```

复测时先看 `buildfs` 输出是否包含 `/font_digits_72.bin`，再看设备日志是否还出现 `font file missing`。如果仍缺失，问题不在固件编译，而在 `uploadfs` 没成功或刷入的不是当前设备。

## 2026-06-06：FontTestState 启动后约 20 秒才显示

### 现象

启用 `ENABLE_FONT_TEST_BOOT` 后，设备先显示白屏，约 20 秒后才出现 `FontTestState` 的文本内容。

关键串口日志：

```text
[StateTrace] beginWithStates start initial=4 t=1163
[StateTrace] NetworkService.begin     t=1164 step=0
[FontTest] onEnter start t=1169
[WiFi] 尝试自动连接: iRadio_2.4G
[FontManager] loaded zhMain glyphs=3815 indexBytes=61040 psram=1 dur=304
=== NTP 时间同步 ===
.........[FontTest] line=zhMain ... probeMs=2615 measureMs=2262
[FontTest] line=zhMain done totalMs=9410
[FontTest] line=zhSub done totalMs=9326
[FontTest] draw screen end dur=19606
[BootTrace] first loop display done ... total=20470
```

### 验证链路

`FontManager` 加载中文主字体只用了约 304 ms，中文副字体约 275 ms，说明真正耗时不在 bin 打开或索引缓存本身。

但 `FontTestState` 的中文行测宽和绘制被拉到 9 秒级，并且日志中间穿插了 WiFi 自动连接和 NTP 等待。`NetworkService::begin()` 在 `StateManager::beginWithStates()` 开头创建 `wifiTask`，该任务优先级高于 Arduino 主循环，创建后会在首帧完成前抢占执行 `WiFiConfig::begin()` 和 `syncNTP()`。

### 根因

首帧显示流程顺序错误：后台网络服务在初始状态 `onEnter()` 之前启动。`FontTestState::onEnter()` 还在同步加载字体、测宽、绘制帧缓冲时，WiFi/NTP 任务已经开始运行并反复抢占主任务。由于 `gui.display()` 只有在 `setup()` 返回后的 `loop()` 中才执行，屏幕一直停留在前一次白底，直到字体绘制和网络/NTP 抢占都结束。

### 修复方式

将后台服务启动从 `StateManager::beginWithStates()` 拆出为 `StateManager::beginBackgroundServices()`，并在 `loop()` 第一次 `gui.display()` 完成后再调用。

修复后的顺序：

```text
setup()
  FontManager::init()
  StateManager::beginWithStates(...)
    register states
    initialState.onEnter()   # 先完成首屏帧缓冲绘制
loop()
  gui.display()              # 首帧先刷到屏幕
  beginBackgroundServices()  # 再启动 WiFi/NTP/Weather 等后台任务
```

复测时重点看：

```text
[BootTrace] first loop display done ...
[StateTrace] background services start ...
```

`background services start` 必须晚于 `first loop display done`。

### 二次复测结论

后台服务延后后，串口顺序已经正确：

```text
[BootTrace] first loop display done t=11488 total=11177
[StateTrace] background services start t=11488
```

这说明网络/NTP 不再阻塞首帧。但 `FontTestState` 仍需约 10 秒才完成首屏绘制：

```text
[FontTest] line=zhMain ... measureMs=2530
[FontTest] draw=zhMain ... drawMs=2259
[FontTest] line=zhMain done totalMs=4789
[FontTest] line=zhSub ... measureMs=2535
[FontTest] draw=zhSub ... drawMs=2257
[FontTest] line=zhSub done totalMs=4792
[FontTest] draw screen end dur=10291
```

新的卡点不在字体加载。中文主字体加载约 267 ms，中文副字体约 275 ms；秒级耗时集中在中文行的测宽和绘制。

根因是字体读取路径过重：`Gui::measureTextWidth()` 只是为了拿 `advanceX`，却通过 `Font.getGlyph()` 读取了完整 bitmap；`Gui::drawText()` 每个字形也会从 SPIFFS 做一次小块随机 `seek/read`。中文 11 个字在测宽和绘制各读一遍 bitmap，因此每行被放大到约 4.8 秒。

修复方式：

- `Font` 新增 `getGlyphMetrics`，测宽路径只查索引 entry，不读取 bitmap。
- `FontRuntime` 增加可选整段 bitmap PSRAM cache；加载字体时一次顺序读取 bitmap 区。
- `FontBinLoader::getGlyph()` 在 bitmap cache 存在时直接从 PSRAM 拷贝单字 bitmap，PSRAM 不足时自动退回原 SPIFFS 按需读取。
- `FontManager` 保持 LRU，新增单字体 bitmap cache 上限，避免强制常驻所有字体 bitmap。

复测时重点看：

```text
[FontManager] loaded zhMain ... bitmapBytes=... bitmapPsram=1
[FontTest] line=zhMain ... measureMs=...
[FontTest] draw=zhMain ... drawMs=...
```

期望 `measureMs` 不再是秒级；如果 `bitmapPsram=1`，中文 `drawMs` 也不应再出现 2 秒级 SPIFFS 随机读耗时。

### 最终复盘

实机复测确认字体显示时间长的问题已解决。本次不是单一原因，而是两个串联问题：

1. 启动顺序问题：后台网络服务在首帧之前启动，WiFi 自动连接和 NTP 同步会抢占主任务。表现为首帧 `gui.display()` 之前日志穿插 `[WiFi]`、`NTP`，屏幕保持白底直到 `FontTestState::onEnter()` 和后台抢占都结束。
2. 字体读取路径问题：网络延后后仍有约 10 秒等待，日志显示 `FontManager` 加载中文字体只需约 270 ms，但中文行 `measureMs` 和 `drawMs` 都是秒级。因此卡点不在字体文件打开，而在 `measureTextWidth()` 和 `drawText()` 逐字读取 bitmap。

最终修复分三步：

- 后台服务改为首帧 `gui.display()` 之后启动，保证用户先看到初始界面。
- `measureTextWidth()` 改为优先走 `Font.getGlyphMetrics`，只查索引和 `advanceX`，不读取 bitmap。
- 字体加载时在预算允许的情况下把 bitmap 区顺序读入 PSRAM；绘制时从 PSRAM 拷贝单字 bitmap，避免 SPIFFS 小块随机 `seek/read`。

后续如果再遇到“字体页白屏等待”：

- 先确认 `[StateTrace] background services start` 晚于 `[BootTrace] first loop display done`。
- 再看中文行 `measureMs/drawMs`。如果 `measureMs` 秒级，优先查测宽是否退回了 `getGlyph()`；如果 `drawMs` 秒级，优先查 `bitmapPsram` 是否为 1，以及 PSRAM bitmap cache 是否因预算或内存不足退回 SPIFFS。

## 2026-06-04：FontTestState 切入后没有任何字体显示

### 现象

打开字体测试启动宏后，设备进入 `FontTestState`，但屏幕没有任何文字。

串口日志显示：

```text
[Fonts] project fonts incomplete: failedPath=/font_zh_main_24.bin error=read header failed
[Fonts] SPIFFS files:
[Fonts]   font24.bin size=336752
[Fonts]   FONT_16_PHT3_65_M.bin size=184432
[Fonts]   FONT_72_PHT3_75.bin size=2529248
[Fonts]   FONT_18_PHT3_65_M.bin size=267494
[Fonts]   FONT_20_PHT3_65_M.bin size=290624
[Fonts]   FONT_22_PHT3_65_M.bin size=313542
[FontTest] ready=0 zhMain=0x0 zhSub=0x0 enMain=0x0 enSub=0x0
```

### 验证链路

本地 `pio run -e esp32s3box -t buildfs` 生成的 SPIFFS 镜像包含当前项目需要的字体文件：

```text
/font_digits_60.bin
/font_en_main_18.bin
/font_en_sub_16.bin
/font_zh_main_24.bin
/font_zh_sub_20.bin
```

设备端日志列出的却是旧 `font24.bin` 和 `FONT_*.bin`。因此问题不在 `FontTestState` 绘制，也不是 `Gui::drawText()` 没执行，而是设备端 SPIFFS 没更新。

### 根因

固件已经切到新字体路径，但设备端 SPIFFS 分区仍保留旧字体文件系统。当前架构下 `FontManager::init()` 可以完成 SPIFFS 挂载，但首次绘制时 `FontBinLoader` 找不到当前项目需要的 `font_*.bin`，测试页会按保护逻辑跳过不可用字体。

### 修复方式

先关闭串口监视器、IDE 串口终端或其他占用 COM 口的程序，然后刷入 SPIFFS：

```bash
pio run -e esp32s3box -t uploadfs
```

如果自动串口选择失败，指定端口：

```bash
pio run -e esp32s3box -t uploadfs --upload-port COM3
```

成功后串口应看到：

```text
[Fonts] expected bins: zhMain=1 zhSub=1 enMain=1 enSub=1 digits60=1
[Fonts] all project fonts ready
[FontTest] ready=1
```

如果仍看到 `Could not open COM3`，说明上传阶段连串口都没有打开成功。先断开 `pio device monitor`、Arduino 串口监视器、VSCode/IDE Monitor，必要时重新插拔设备后再执行 `uploadfs`。

## 2026-06-03：16 号 bin 字体“我”看起来接近 18 号

### 现象

`FontBinTestState` 中对比 `bin` 和 `raw` 后，16 号 bin 的“我”视觉上偏大，接近 18 号；raw 静态取模的 16 号则明显更小。

### 验证链路

直接从 `data/FONT_16_PHT3_65_M.bin` 抽取 U+6211 的 index entry 和 bitmap，再与 `Font_chinese_AlibabaPuHuiTi_3_65_m_Medium_16_16_FontTest.cpp` 中的静态数组逐字节对比。

修复前结果：

```text
16 bin bbox=(1,1)-(14,14), pixels=120
16 raw bbox=(2,2)-(13,13), pixels=90
same bytes: False
```

同时对 18/20/22 做同样检查，三个字号的 bin 与 raw 均逐字节一致。因此问题收敛到 16px 生成规则，不是运行时绘制、SPIFFS 读取或拍照观感导致。

### 根因

`tools/gen_font24_bin.py` 中中文渲染配置使用：

```python
cell_margin=max(1, round(font_size / 12))
```

当 `font_size=16` 时，`cell_margin=1`，中文 glyph 被缩放到 `14x14` 可用区域；而 `FontTestState` 的静态 C 数组取模效果等价于左右/上下至少 2px 边距，即 16px 字形使用 `12x12` 可视区域。

18/20/22 不受影响，是因为它们计算后本来就是 2px 边距。

### 修复

将中文最小边距改成 2px：

```python
cell_margin=max(2, round(font_size / 12))
```

重新生成 16 号 bin：

```bash
python tools/gen_font24_bin.py --font fonts/AlibabaPuHuiTi-3-65-m-Medium.otf --size 16 --no-uploadfs
```

修复后再次抽取 U+6211 比对：

```text
16 metrics (16, 16, 2, 16) same True
18 metrics (18, 18, 3, 18) same True
20 metrics (20, 20, 3, 20) same True
22 metrics (22, 22, 3, 22) same True
```

最后上传 SPIFFS：

```bash
pio run -e esp32s3box -t uploadfs
```

上传日志确认包含：

```text
/FONT_16_PHT3_65_M.bin
/FONT_18_PHT3_65_M.bin
/FONT_20_PHT3_65_M.bin
/FONT_22_PHT3_65_M.bin
/FONT_72_PHT3_75.bin
Hash of data verified.
```

## 2026-06-02：FontBinTestState 显示 font24.bin missing

### 现象

开机已切到 `FontBinTestState` 后，页面显示：

```text
font24.bin missing
```

代码路径是：

- `src/main.cpp` 中先执行 `SPIFFS.begin(false)`
- 然后执行 `gFont24Spiffs.begin(SPIFFS, "/font24.bin")`
- `FontBinTestState::drawScreen()` 中如果 `gFont24Spiffs.font()` 返回 `nullptr`，就显示 `font24.bin missing`

因此该现象只说明运行时字体对象不可用，不等价于仓库里一定缺少 `font24.bin`。

### 排查步骤

先拆成三个阶段确认：

1. 本地资源是否存在
2. `font24.bin` 格式是否能通过 `Font24Spiffs` 的严格头部校验
3. 设备端 SPIFFS 分区是否已经刷入该文件

本次检查结果：

```text
data/font24.bin 存在
文件大小：336752 bytes
```

执行：

```bash
pio run -e esp32s3box -t buildfs
```

输出中能看到：

```text
Building FS image from 'data' directory to .pio\build\esp32s3box\spiffs.bin
/font24.bin
```

说明 PlatformIO 能把 `data/font24.bin` 正确打包进 SPIFFS 镜像。

本地头部元数据也与 `Font24Spiffs.cpp` 的校验规则一致：

```text
magic=F24BIN1
version=1
header=64
entry=16
flags=0x0003
glyphs=3850
index=64
bitmap=61664
declared=336752
actual=336752
lineHeight=24
maxGlyphBytes=72
```

因此可以排除：

- 仓库缺少 `data/font24.bin`
- `font24.bin` 没被 `buildfs` 打包
- 当前 `font24.bin` 头部格式与 `Font24Spiffs` 不兼容

### 根因

设备之前只刷了应用固件，没有刷 SPIFFS 文件系统分区。

`pio run -e esp32s3box -t upload` 只上传固件，不会自动上传 `data/` 生成的 SPIFFS 镜像。固件运行后 `SPIFFS.begin(false)` 可能成功挂载空分区或旧分区，但 `SPIFFS.open("/font24.bin", "r")` 找不到文件，最终 `gFont24Spiffs.font()` 返回 `nullptr`。

### 解决

执行：

```bash
pio run -e esp32s3box -t uploadfs
```

本次上传结果确认：

```text
Building FS image from 'data' directory to .pio\build\esp32s3box\spiffs.bin
/font24.bin
Uploading .pio\build\esp32s3box\spiffs.bin
Flash will be erased from 0x00810000 to 0x00ffffff
Hash of data verified.
Hard resetting via RTS pin.
```

上传成功后，设备端 SPIFFS 分区包含 `/font24.bin`，字体测试页不应再显示 `font24.bin missing`。

### 后续规则

只要满足下面任一情况，都需要刷 SPIFFS：

- 首次烧录新开发板
- 修改了 `data/` 目录内容
- 重新生成了 `data/font24.bin`
- 擦除了整片 Flash 或更换了分区表

常用顺序：

```bash
pio run -e esp32s3box
pio run -e esp32s3box -t upload
pio run -e esp32s3box -t uploadfs
```

如果再次出现该问题，优先看串口日志：

```text
[Font24] SPIFFS mount failed
[Font24] init failed: open font file failed
[Font24] /font24.bin ready
```

其中：

- `SPIFFS mount failed`：先查分区表、Flash、是否需要格式化或重新刷分区
- `open font file failed`：优先执行 `buildfs` 和 `uploadfs`
- `/font24.bin ready`：字体文件已加载，后续再查字形缺失或绘制链路
