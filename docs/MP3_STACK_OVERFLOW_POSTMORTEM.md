# MP3 解码栈溢出复盘

## 结论

本次 MP3 播放失败的根因不是 SD 卡、不是 I2S、也不是 MP3 文件本身，而是 `Mp3Decoder` 任务栈空间不足。

触发点在 `minimp3` 解码链路：

- 我们自己的临时 PCM 缓冲 `pcmTemp` 最初放在 `decoderTaskFunc()` 栈上
- `decoderTaskFunc()` 内还有一个栈上的 `mp3dec_t dec`
- `minimp3` 的 `mp3dec_decode_frame()` 内部还会创建一个很大的 `mp3dec_scratch_t` 局部工作区

三者叠加后，原先 `Mp3Decoder` 的任务栈不够，最终触发：

```text
Stack canary watchpoint triggered (Mp3Decoder)
```

最终修复方式是两步：

1. 把 `pcmTemp` 和 `mp3dec_t` 都移出任务栈，改为堆上长期缓冲
2. 把 `Mp3Decoder` 的任务栈从 `8192` 逐步提升到 `32768`，覆盖 `minimp3` 内部解码临时栈消耗

---

## 问题现象

串口日志表现为：

```text
[MP3] play() request: /music/yujian.mp3
[MP3] Playing: /music/yujian.mp3 (4961730 bytes)
[MP3] Decoder task start
[MP3] Writer task start
[MP3] play() started tasks: /music/yujian.mp3
[MP3 TEST] Track started: /music/yujian.mp3
Guru Meditation Error: Core  1 panic'ed
Debug exception reason: Stack canary watchpoint triggered (Mp3Decoder)
```

这说明：

- 文件已经成功打开
- 解码任务和写出任务已经创建成功
- 崩溃发生在播放阶段内部
- 崩溃线程明确是 `Mp3Decoder`

因此排查应聚焦：

- 解码任务的局部变量
- 解码库内部栈占用
- 任务栈配置是否过小

---

## 第一轮判断

最开始先怀疑的是 `pcmTemp`：

```cpp
int16_t pcmTemp[MINIMP3_MAX_SAMPLES_PER_FRAME];
```

其中：

- `MINIMP3_MAX_SAMPLES_PER_FRAME = 1152 * 2 = 2304`
- `2304 * sizeof(int16_t) = 4608 bytes`

也就是说，光这一个局部数组就会吃掉约 `4.5KB` 栈。

原来的 `Mp3Decoder` 栈是 `8192`，再叠加：

- `mp3dec_t dec`
- `mp3dec_frame_info_t info`
- 函数调用栈
- 日志调用栈

已经非常危险。

因此第一轮修复先把 `pcmTemp` 挪到堆上。

---

## 为什么第一轮修复后仍然溢出

把 `pcmTemp` 挪走后，问题并没有完全消失，说明任务栈里还有大对象。

继续看 `minimp3.h` 可以发现：

### 1. `mp3dec_t` 本身不小

```cpp
typedef struct
{
    float mdct_overlap[2][9*32], qmf_state[15*2*32];
    int reserv, free_format_bytes;
    unsigned char header[4], reserv_buf[511];
} mp3dec_t;
```

这个结构体内部带大量浮点状态缓存，如果把它定义在任务函数里，就会继续占用解码任务栈。

### 2. `mp3dec_decode_frame()` 内部还有更大的 `mp3dec_scratch_t`

`minimp3` 内部还有：

```cpp
typedef struct
{
    bs_t bs;
    uint8_t maindata[MAX_BITRESERVOIR_BYTES + MAX_L3_FRAME_PAYLOAD_BYTES];
    L3_gr_info_t gr_info[4];
    float grbuf[2][576], scf[40], syn[18 + 15][2*32];
    uint8_t ist_pos[2][39];
} mp3dec_scratch_t;
```

这个工作区是 `mp3dec_decode_frame()` 内部的局部变量，也就是说：

- 无法仅靠把业务层缓冲挪到堆上彻底解决
- 必须同时保证 `Mp3Decoder` 任务本身有足够大的任务栈

这就是为什么把 `pcmTemp` 挪走后，仍然出现 `Mp3Decoder` stack canary。

---

## 最终修复

### 修复 1：`pcmTemp` 挪到堆上

把原来任务栈上的：

```cpp
int16_t pcmTemp[MINIMP3_MAX_SAMPLES_PER_FRAME];
```

改成 `Mp3Player` 成员：

- `pcmTemp_`

并在 `begin()` 中分配，在析构中释放。

作用：

- 释放 `4.5KB` 左右任务栈
- 避免每次进入任务都重复占用这块固定缓冲

### 修复 2：`mp3dec_t` 挪到堆上

把原来 `decoderTaskFunc()` 里的：

```cpp
mp3dec_t dec;
```

改成 `Mp3Player` 成员：

- `decoderState_`

并在 `begin()` 中分配，在析构中释放。

作用：

- 避免解码器状态长期压在任务栈上
- 让任务栈只保留必要的流程控制变量

### 修复 3：放大 `Mp3Decoder` 任务栈

由于 `minimp3` 内部还会临时创建 `mp3dec_scratch_t`，因此业务层对象移出任务栈后，仍需提升任务栈。

本次最终把：

- `Mp3Decoder` 栈从 `8192`
- 调整到 `12288`
- 最终调整到 `32768`

这样可以覆盖：

- `minimp3` 内部解码临时工作区
- 本任务局部变量
- 函数调用开销
- 日志输出开销

---

## 为什么 `pcmBuf_` 不是根因

这次排查中一个容易误判的点是：代码里已经给 `pcmBuf_` 和 `readBuf_` 分配了 PSRAM，看起来“缓冲区已经很大了，为何还会栈溢出”。

关键区别是：

- `pcmBuf_`、`readBuf_`、`pcmTemp_`、`decoderState_` 属于堆内存
- `Mp3Decoder` 的 stack canary 检测的是任务栈

也就是说：

- 堆上缓冲再大，也不能自动缓解任务栈溢出
- 只要函数里还有大局部变量，栈一样会爆

这次问题本质上是：

- “解码业务缓冲” 和 “解码任务栈” 是两套不同资源
- 之前只看到了前者，没有把后者单独核算

---

## 经验教训

### 1. 嵌入式里要把“堆内存”和“任务栈”分开算

看到 PSRAM 分配成功，并不代表任务函数就安全。

尤其在 FreeRTOS 下，要单独检查：

- 任务栈大小
- 任务函数中的大局部变量
- 第三方库是否在函数内部声明大结构体

### 2. 第三方库的内部栈占用必须读源码确认

只看业务层代码，容易漏掉像 `minimp3` 这种“库内部临时工作区很大”的情况。

以后遇到类似问题，必须继续往下看：

- `.h` 里的结构体大小
- `.c/.cpp` 里的局部对象
- 是否存在 `scratch`、`workspace`、`temp` 这类局部大块内存

### 3. 不要只靠不断加大任务栈硬扛

更稳妥的顺序应当是：

1. 先把可迁移的大对象移出任务栈
2. 再根据库的真实内部占用调任务栈

这样比“盲目从 8KB 改到 32KB”更可控，也更利于后续维护。

### 4. 串口日志在这类问题中非常关键

本次能快速收敛，核心是日志明确给出：

- 文件已经打开
- 任务已经启动
- 崩溃线程是 `Mp3Decoder`

这直接把排查范围缩到解码任务本身，而不是 SD、I2S、文件格式。

---

## 后续建议

如果后面还要继续增强 MP3 播放模块，建议优先做这几件事：

1. 在 `Mp3Decoder` 和 `Mp3Writer` 关键节点打印 `uxTaskGetStackHighWaterMark()`，长期观察栈水位
2. 对 `begin()` 的多次调用增加防重复分配保护
3. 对不同码率、不同采样率的 MP3 分别做回归测试
4. 若后续还要上更复杂解码库，优先提前核算“库内部 scratch 栈占用”

