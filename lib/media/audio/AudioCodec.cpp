/**
 * @file AudioCodec.cpp
 * @brief 音频编解码器高层控制器 —— 协调 ES7210（麦克风 ADC）+ ES8311（扬声器 DAC）
 *
 * ══════════════════════════════════════════════════════════════
 *  硬件拓扑（数据流向）
 * ══════════════════════════════════════════════════════════════
 *
 *   MIC1/MIC3 ──→ ES7210(ADC) ──I2S-DIN(GPIO10)──→ ESP32 I2S RX
 *                                                       │
 *                                                   录音缓冲区(PSRAM)
 *                                                       │  软件增益
 *                                                       ▼
 *   Speaker ←── PA(GPIO46) ←── ES8311(DAC) ←──I2S-DOUT(GPIO8)──← ESP32 I2S TX
 *
 * ══════════════════════════════════════════════════════════════
 *  核心设计：先录后放（Record-then-Play）
 * ══════════════════════════════════════════════════════════════
 *
 *  为什么不做实时回环（mic → speaker loopback）？
 *  因为 MIC 和喇叭距离很近，实时回环会形成声学反馈环路：
 *    speaker 发声 → mic 拾音 → 放大 → speaker 发声 → mic 拾音 → ...
 *  每一轮信号被增益放大，几毫秒内即可指数增长至削波 → 产生刺耳啸叫。
 *
 *  解决方案（参考官方 Demo）：
 *    录音阶段：PA 关闭（speaker 静音），纯粹采集 mic 信号
 *    播放阶段：将录制数据一次性写入 I2S TX，此时不读 mic
 *  两个阶段互斥，从物理上切断反馈环路。
 *
 * ══════════════════════════════════════════════════════════════
 *  I2S 数据格式
 * ══════════════════════════════════════════════════════════════
 *
 *  I2S 配置：32-bit stereo（每帧 = L[32bit] + R[32bit] = 8 字节）
 *  ES7210 输出 16-bit 音频数据，左对齐放在 32-bit slot 的高 16 位：
 *    | 16-bit PCM sample | 16-bit padding (0) |
 *    |<-------- 32-bit I2S word ------------->|
 *
 *  因此提取样本：int16_t sample = (int16_t)(word >> 16);
 *  写回样本：    word = ((int32_t)sample) << 16;
 *
 *  码率 = 16000(采样率) × 2(声道) × 4(字节/样本) = 128 KB/s
 *  3秒录音 = 384 KB，存入 PSRAM。
 */

#include "AudioCodec.h"
#include <media/audio/Es8311Init.h>
#include "Es7210Init.h"
#include <media/audio/I2sDriver.h>
#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <Wire.h>

#define AUDIO_I2S_PORT  I2S_NUM_0

// ═══════════════════════════════════════════════════════════════
//  begin() —— 初始化整个音频子系统
// ═══════════════════════════════════════════════════════════════
//  调用顺序很重要：
//    1. initEs8311() —— 通过 I2C 配置 DAC 芯片的寄存器（时钟、格式、音量等）
//    2. initEs7210() —— 通过 I2C 配置 ADC 芯片的寄存器（MIC 选择、增益等）
//    3. initI2s()    —— 安装 ESP32 I2S 驱动，配置引脚，开始产生 MCLK/BCLK/WS
//  必须在 Wire.begin(13, 14) 之后调用（I2C 总线已就绪）。
bool AudioCodec::begin(int sampleRate) {
    if (!initEs8311(sampleRate)) {
        Serial.println("[Audio] ES8311 init failed");
        return false;
    }
    if (!initEs7210()) {
        Serial.println("[Audio] ES7210 init failed");
        return false;
    }
    if (!initI2s(sampleRate)) {
        Serial.println("[Audio] I2S init failed");
        return false;
    }
    initOk_ = true;
    Serial.println("[Audio] Codec OK");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  startRecordPlay() —— 启动「先录后放」任务
// ═══════════════════════════════════════════════════════════════
//  创建一个 FreeRTOS 任务在 Core 1 上运行，依次执行：
//    Phase 1: 录音 recMs 毫秒（PA 关闭）
//    Phase 2: 软件增益处理
//    Phase 3: 播放（PA 开启）
//  任务完成后自动退出，running_ 置 false。
void AudioCodec::startRecordPlay(int recMs) {
    if (!initOk_ || running_) return;   // 防止重复启动
    stopRequested_ = false;
    recMs_ = recMs;

    xTaskCreatePinnedToCore(recordPlayTask, "AudioRecPlay",
                            4096,   // 栈大小（字节），4KB 足够
                            this,   // 传入 this 指针作为任务参数
                            5,      // 优先级（高于 loop 的 1）
                            &taskHandle_,
                            1);     // 绑定到 Core 1（Core 0 留给 WiFi/BT）
}

// ═══════════════════════════════════════════════════════════════
//  stop() —— 请求停止正在运行的任务，并等待其退出
// ═══════════════════════════════════════════════════════════════
//  设置 stopRequested_ 标志，任务会在下一次循环检查时退出。
//  阻塞等待最多 2 秒（200 × 10ms）确保任务完全退出后再操作硬件。
void AudioCodec::stop() {
    if (!running_) return;
    stopRequested_ = true;
    for (int i = 0; i < 200 && running_; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    taskHandle_ = nullptr;

    // 确保硬件处于安全状态：关闭功放、静音 DAC
    setSpeakerEnabled(false);
    es8311SetDacVolume(0x00);
}

// ═══════════════════════════════════════════════════════════════
//  setSpeakerEnabled() —— 控制功放使能引脚
// ═══════════════════════════════════════════════════════════════
//  GPIO46 → PA（功率放大器）使能，高电平开启。
//  录音时必须关闭 PA，否则喇叭残留噪声会被 MIC 拾取。
void AudioCodec::setSpeakerEnabled(bool enable) {
    digitalWrite(46, enable ? HIGH : LOW);
}

// ═══════════════════════════════════════════════════════════════
//  recordPlayTask() —— FreeRTOS 任务：先录后放
// ═══════════════════════════════════════════════════════════════
//
//  时序图：
//    ┌─────────────┐     ┌──────────┐     ┌─────────────┐
//    │ Phase 1     │     │ 软件增益  │     │ Phase 2     │
//    │ 录音 3000ms │ ──→ │ 16x 放大  │ ──→ │ 播放        │
//    │ PA=OFF      │     │ 带削波保护│     │ PA=ON       │
//    └─────────────┘     └──────────┘     └─────────────┘
//         ↑ i2s_read()                         ↑ i2s_write()
//         │                                    │
//    ES7210 → I2S RX                    I2S TX → ES8311 → PA → Speaker
//
void AudioCodec::recordPlayTask(void* arg) {
    AudioCodec* self = static_cast<AudioCodec*>(arg);
    self->running_ = true;

    // ── 计算缓冲区大小 ──
    // 16kHz 采样率 × 2 声道 × 4 字节/样本 = 128 字节/毫秒
    const int bytesPerMs = 16000 * 2 * 4 / 1000;  // = 128
    int totalBytes = self->recMs_ * bytesPerMs;    // 3000ms = 384000 字节 ≈ 375KB

    // 优先使用 PSRAM（8MB），内部 RAM 不够 375KB
    uint8_t* recBuf = (uint8_t*)heap_caps_malloc(totalBytes, MALLOC_CAP_SPIRAM);
    if (!recBuf) {
        recBuf = (uint8_t*)heap_caps_malloc(totalBytes, MALLOC_CAP_INTERNAL);
    }
    if (!recBuf) {
        Serial.printf("[RecPlay] alloc %d bytes failed\n", totalBytes);
        self->running_ = false;
        vTaskDelete(nullptr);
        return;
    }

    // ══════════════════════════════════════════════════════════
    //  Phase 1: 录音（PA 关闭，切断声学反馈环路）
    // ══════════════════════════════════════════════════════════
    self->setSpeakerEnabled(false);   // 关闭功放，喇叭静音
    es8311SetDacVolume(0x00);         // DAC 静音，双重保险

    // 清空 I2S DMA 缓冲区中的残留数据（上一次播放遗留的）
    // 如果不清空，录音开头会混入上次播放的尾巴
    i2s_zero_dma_buffer(AUDIO_I2S_PORT);
    {
        // 额外读取并丢弃 10 块数据（10 × 512B = 5KB），
        // 确保 DMA FIFO 中的旧数据被完全排空
        uint8_t trash[512];
        size_t n = 0;
        for (int i = 0; i < 10; i++) {
            i2s_read(AUDIO_I2S_PORT, trash, sizeof(trash), &n, pdMS_TO_TICKS(10));
        }
    }

    Serial.printf("[RecPlay] Recording %d ms...\n", self->recMs_);
    int recorded = 0;
    while (recorded < totalBytes) {
        if (self->stopRequested_) break;   // 支持中途取消

        size_t bytesRead = 0;
        int want = totalBytes - recorded;
        if (want > 512) want = 512;        // 每次最多读 512 字节（64 帧）

        // i2s_read(): 从 I2S RX DMA 缓冲区读取 ES7210 采集的音频数据
        // 超时 50ms：16kHz 下 512B 只需 4ms，50ms 足够等待
        esp_err_t r = i2s_read(AUDIO_I2S_PORT, recBuf + recorded, want,
                               &bytesRead, pdMS_TO_TICKS(50));
        if (r != ESP_OK && r != ESP_ERR_TIMEOUT) break;
        recorded += (int)bytesRead;
    }
    Serial.printf("[RecPlay] Recorded %d bytes\n", recorded);

    if (self->stopRequested_) {
        free(recBuf);
        self->running_ = false;
        vTaskDelete(nullptr);
        return;
    }

    // ══════════════════════════════════════════════════════════
    //  软件增益处理（16 倍放大，约 +24dB）
    // ══════════════════════════════════════════════════════════
    //
    //  为什么需要软件增益？
    //  ES7210 的模拟增益最大 36dB，加上 MIC 灵敏度，原始信号幅度通常较小。
    //  DAC 端 ES8311 的 REG32=0xBF 是 0dB（不增不减），不额外放大。
    //  因此在软件层对每个样本乘以 16，等效于 +24dB 的数字增益。
    //
    //  削波保护：乘法结果用 int32_t 承接（不会溢出），
    //  然后 clamp 到 [-32768, 32767] 范围内，防止溢出产生爆音。
    //
    //  数据格式回顾：
    //    每个 int32_t I2S word 的高 16 位是实际 PCM 样本
    //    提取: sample = (int16_t)(word >> 16)
    //    写回: word = ((int32_t)sample) << 16
    {
        int32_t* p = reinterpret_cast<int32_t*>(recBuf);
        int nWords = recorded / 4;   // 每个 I2S word 4 字节
        for (int i = 0; i < nWords; i++) {
            int16_t sample = (int16_t)(p[i] >> 16);       // 提取高 16 位 PCM 样本
            int32_t boosted = (int32_t)sample * 16;        // 16 倍放大
            if (boosted > 32767)  boosted = 32767;         // 正向削波保护
            if (boosted < -32768) boosted = -32768;        // 负向削波保护
            p[i] = ((int32_t)(int16_t)boosted) << 16;      // 写回高 16 位
        }
    }

    // ══════════════════════════════════════════════════════════
    //  Phase 2: 播放（PA 开启）
    // ══════════════════════════════════════════════════════════
    Serial.println("[RecPlay] Playing back...");

    // 清空 TX DMA 缓冲区，避免播放开头有一小段噪声
    i2s_zero_dma_buffer(AUDIO_I2S_PORT);

    // 设置 DAC 音量为 0dB（0xBF），然后开启功放
    // 注意顺序：先设音量再开 PA，避免 PA 开启瞬间放大 DAC 噪声
    es8311SetDacVolume(0xBF);
    self->setSpeakerEnabled(true);

    int played = 0;
    while (played < recorded) {
        if (self->stopRequested_) break;

        size_t bytesWritten = 0;
        int want = recorded - played;
        if (want > 512) want = 512;

        // i2s_write(): 将音频数据写入 I2S TX DMA 缓冲区，由硬件发送给 ES8311
        esp_err_t r = i2s_write(AUDIO_I2S_PORT, recBuf + played, want,
                                &bytesWritten, pdMS_TO_TICKS(50));
        if (r != ESP_OK && r != ESP_ERR_TIMEOUT) break;
        played += (int)bytesWritten;
    }

    // ── 播放结束，安全关闭硬件 ──
    // 等待 100ms 让 DMA 中最后一批数据播放完毕
    vTaskDelay(pdMS_TO_TICKS(100));

    // 关闭顺序：先关 PA（切断喇叭），再静音 DAC，再清空 DMA
    // 如果先静音 DAC 再关 PA，PA 在 DAC 切换瞬间可能放大一个 pop 噪声
    self->setSpeakerEnabled(false);
    es8311SetDacVolume(0x00);
    i2s_zero_dma_buffer(AUDIO_I2S_PORT);  // 清空残留，下次录音不会录到旧数据

    free(recBuf);
    Serial.printf("[RecPlay] Done. Played %d bytes.\n", played);

    self->running_ = false;
    vTaskDelete(nullptr);   // 任务自毁，FreeRTOS 回收资源
}
