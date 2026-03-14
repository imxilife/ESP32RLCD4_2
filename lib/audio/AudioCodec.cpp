#include "AudioCodec.h"
#include "Es8311Init.h"
#include "Es7210Init.h"
#include "I2sDriver.h"
#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

#define AUDIO_I2S_PORT  I2S_NUM_0

// ── begin ─────────────────────────────────────────────────────
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

// ── startEcho ────────────────────────────────────────────────
void AudioCodec::startEcho() {
    if (!initOk_ || running_) return;
    stopRequested_ = false;
    xTaskCreatePinnedToCore(echoTask, "AudioEcho",
                            4096,   // stack bytes
                            this,
                            5,      // priority
                            &taskHandle_,
                            1);     // core 1
}

// ── stopEcho ─────────────────────────────────────────────────
void AudioCodec::stopEcho() {
    if (!running_) return;
    stopRequested_ = true;
    // Wait up to 2 s for task to exit cleanly (normally < 40 ms)
    for (int i = 0; i < 200 && running_; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    taskHandle_ = nullptr;
}

// ── setSpeakerEnabled ────────────────────────────────────────
void AudioCodec::setSpeakerEnabled(bool enable) {
    digitalWrite(46, enable ? HIGH : LOW);
}

// ── diagMicInput ─────────────────────────────────────────────
// I2S 为 32-bit stereo，ES7210 输出 16-bit 数据左对齐放在高 16 位。
// 读取一段时间的 I2S RX 数据，逐块打印 L 声道峰值，确认录音链路是否有信号。
void AudioCodec::diagMicInput(int durationMs) {
    Serial.printf("[MIC] --- Step 1: ES7210 mic input check (%dms) ---\n", durationMs);

    // 对齐分配，避免 uint8_t* 转 int32_t* 的 UB
    static const int FRAMES    = 64;
    static const int BUF_WORDS = FRAMES * 2;          // 64 stereo frames = 128 int32
    int32_t buf[BUF_WORDS];
    const int BUF_BYTES = BUF_WORDS * sizeof(int32_t); // 512 bytes

    uint32_t start   = millis();
    int      chunk   = 0;
    int16_t  maxPeak = 0;

    while ((int)(millis() - start) < durationMs) {
        size_t   bytesRead = 0;
        esp_err_t r = i2s_read(AUDIO_I2S_PORT, buf, BUF_BYTES,
                               &bytesRead, pdMS_TO_TICKS(50));
        if (r != ESP_OK && r != ESP_ERR_TIMEOUT) {
            Serial.printf("[MIC]   i2s_read err=0x%X, stop\n", r);
            break;
        }
        if (bytesRead == 0) continue;

        // 16-bit audio 在 32-bit 帧的高 16 位，取 L 声道峰值
        int16_t peak  = 0;
        int     nFrames = (int)bytesRead / 8;  // 每帧 2ch * 4B = 8B
        for (int f = 0; f < nFrames; f++) {
            int16_t s = (int16_t)(buf[f * 2] >> 16);   // L channel
            int16_t a = (s < 0) ? -s : s;
            if (a > peak) peak = a;
        }
        if (peak > maxPeak) maxPeak = peak;
        Serial.printf("[MIC]   chunk%02d: %d bytes  peak=%d\n", chunk++, (int)bytesRead, peak);
    }

    Serial.printf("[MIC] Max peak = %d  => %s\n", maxPeak,
                  maxPeak > 200 ? "SIGNAL OK" : "SILENT / NO SIGNAL");
    Serial.println("[MIC] --- done ---");
}

// ── diagSpeakerOutput ────────────────────────────────────────
// 验证 ES8311 输出链路：先打印 PA 引脚状态，再写入 1kHz 方波，
// 打印每块实际写入字节，确认 I2S TX 是否正常工作。
// 1kHz @ 16kHz 采样率：每半周期 8 帧切换一次极性。
void AudioCodec::diagSpeakerOutput(int durationMs) {
    Serial.printf("[SPK] --- Step 2: ES8311 speaker output check (%dms) ---\n", durationMs);

    // PA 引脚状态检查
    int paState = digitalRead(46);
    Serial.printf("[SPK] PA pin (GPIO46) = %s\n", paState ? "HIGH (PA on)" : "LOW (PA off!)");

    static const int FRAMES    = 64;
    static const int BUF_WORDS = FRAMES * 2;
    int32_t buf[BUF_WORDS];
    const int BUF_BYTES = BUF_WORDS * sizeof(int32_t);

    // 1kHz 方波：16-bit 左对齐到 32-bit 高位
    // 正半周 = 0x7FFF0000，负半周 = 0x80000000（= -32768 << 16）
    const int32_t kPOS = (int32_t)0x7FFF0000;
    const int32_t kNEG = (int32_t)0x80000000;

    uint32_t start       = millis();
    int      chunk       = 0;
    int      phase       = 0;   // 0=正, 1=负
    int      phaseCnt    = 0;   // 在当前相位内已走的帧数
    int      totalWritten = 0;

    Serial.printf("[SPK] Writing 1kHz square wave to I2S TX...\n");
    while ((int)(millis() - start) < durationMs) {
        for (int f = 0; f < FRAMES; f++) {
            int32_t v    = (phase == 0) ? kPOS : kNEG;
            buf[f * 2]   = v;  // L
            buf[f * 2+1] = v;  // R
            if (++phaseCnt >= 8) { phaseCnt = 0; phase ^= 1; }
        }

        size_t   written = 0;
        esp_err_t r = i2s_write(AUDIO_I2S_PORT, buf, BUF_BYTES,
                                &written, pdMS_TO_TICKS(50));
        totalWritten += (int)written;
        Serial.printf("[SPK]   chunk%02d: wrote=%d/%d bytes  rc=0x%X\n",
                      chunk++, (int)written, BUF_BYTES, r);
        if (r != ESP_OK) break;
    }

    Serial.printf("[SPK] Total written = %d bytes\n", totalWritten);
    Serial.println("[SPK] --- done ---");
}

// ── echoTask ─────────────────────────────────────────────────
// Reads PCM from ES7210 via I2S RX and writes it to ES8311 via I2S TX.
// Buffer: 512 bytes = 64 stereo 32-bit frames ≈ 4 ms @ 16 kHz.
void AudioCodec::echoTask(void* arg) {
    AudioCodec* self = static_cast<AudioCodec*>(arg);

    const int BUF_BYTES = 512;
    uint8_t* buf = static_cast<uint8_t*>(
        heap_caps_malloc(BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (!buf) {
        Serial.println("[Audio] echo buf alloc failed");
        self->running_ = false;
        vTaskDelete(nullptr);
        return;
    }

    self->running_ = true;

    size_t bytesRead = 0, bytesWritten = 0;
    while (!self->stopRequested_) {
        esp_err_t r = i2s_read(AUDIO_I2S_PORT, buf, BUF_BYTES,
                               &bytesRead, pdMS_TO_TICKS(20));
        if ((r == ESP_OK || r == ESP_ERR_TIMEOUT) && bytesRead > 0) {
            i2s_write(AUDIO_I2S_PORT, buf, bytesRead,
                      &bytesWritten, pdMS_TO_TICKS(20));
        }
    }
    free(buf);
    self->running_ = false;
    vTaskDelete(nullptr);
}
