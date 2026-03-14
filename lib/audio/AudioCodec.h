#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// High-level audio codec controller: ES8311 (speaker) + ES7210 (mic).
// Usage:
//   1. Call begin() once in setup() after Wire.begin().
//   2. Call startEcho() / stopEcho() from MusicPlayerState::onEnter/onExit.
class AudioCodec {
public:
    // Initialize ES8311 + ES7210 via Wire and install I2S driver.
    // Must be called after Wire.begin(). Returns true on success.
    bool begin(int sampleRate = 16000);

    // Spawn FreeRTOS echo task (mic → speaker loopback).
    // No-op if begin() failed or already running.
    void startEcho();

    // Signal echo task to stop and block (up to 2 s) for clean exit.
    void stopEcho();

    bool isRunning() const { return running_; }

    // Drive the PA enable pin (GPIO46).
    void setSpeakerEnabled(bool enable);

    // ── 诊断：两步独立验证 ────────────────────────────────────
    // Step 1: 从 I2S RX 读取 mic 数据，打印每块峰值；验证 ES7210 录音链路。
    void diagMicInput(int durationMs = 500);
    // Step 2: 向 I2S TX 写入 1kHz 方波，打印 PA 状态与写入结果；验证 ES8311 输出链路。
    void diagSpeakerOutput(int durationMs = 500);
    // Step 3: 回读 ES8311 / ES7210 关键寄存器，验证配置是否生效。
    void diagReadRegisters();

private:
    static void echoTask(void* arg);

    TaskHandle_t  taskHandle_    = nullptr;
    volatile bool stopRequested_ = false;
    volatile bool running_       = false;
    bool          initOk_        = false;
};
