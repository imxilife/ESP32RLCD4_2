#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "minimp3.h"

/**
 * MP3 播放器 —— 从 SD 卡读取 MP3 文件，软解码后通过 I2S/ES8311/PA 播放
 *
 * 架构：双缓冲 producer-consumer
 *   Decoder Task (producer): 读 SD → minimp3 解码 → 填充 PCM 缓冲区
 *   Writer Task  (consumer): 从 PCM 缓冲区 → i2s_write() → ES8311 → Speaker
 *
 * 使用前须确保 AudioCodec::begin() 和 SDCard::begin() 已调用。
 */
class Mp3Player {
public:
    struct Impl;
    struct TrackMetrics {
        uint32_t progressMs = 0;
        uint32_t durationMs = 0;
        uint32_t sampleRate = 0;
        uint16_t channels = 0;
        uint16_t bitrateKbps = 0;
        size_t fileSizeBytes = 0;
        bool durationKnown = false;
        bool metadataReady = false;
    };

    Mp3Player();
    ~Mp3Player();

    bool begin();
    bool play(const char* path);
    bool pause();
    bool resume();
    void stop();
    bool isPlaying() const { return playing_; }
    bool isPaused() const { return paused_; }
    bool hasTrackFinished() const { return trackFinished_; }
    uint32_t currentProgressMs() const { return trackMetrics_.progressMs; }
    TrackMetrics trackMetrics() const { return trackMetrics_; }

    // 扫描目录下的 .mp3 文件，返回数量
    int scanMp3Files(const char* dir, String names[], int maxCount);

private:
    // ── 双缓冲 (ping-pong) ──
    static constexpr int kMaxSamplesPerFrame = 1152;
    static constexpr int kPcmBufSamples = kMaxSamplesPerFrame * 2; // stereo
    static constexpr int kPcmBufBytes   = kPcmBufSamples * sizeof(int32_t);

    int32_t* pcmBuf_[2]    = {nullptr, nullptr};
    int      pcmBufLen_[2] = {0, 0};  // 有效字节数，-1 = EOF sentinel

    SemaphoreHandle_t bufReady_[2] = {nullptr, nullptr};
    SemaphoreHandle_t bufFree_[2]  = {nullptr, nullptr};

    // ── MP3 读取缓冲区 ──
    static constexpr int kReadBufSize = 4096;
    uint8_t* readBuf_      = nullptr;
    int      readBufValid_ = 0;
    int      readBufPos_   = 0;
    int16_t* pcmTemp_      = nullptr;
    mp3dec_t* decoderState_ = nullptr;

    // ── 状态 ──
    Impl*             impl_          = nullptr;
    volatile bool     playing_       = false;
    volatile bool     paused_        = false;
    volatile bool     stopRequested_ = false;
    volatile bool     trackFinished_ = false;
    TaskHandle_t      decoderTask_   = nullptr;
    TaskHandle_t      writerTask_    = nullptr;
    int               origSampleRate_ = 16000;
    volatile uint32_t consumedCompressedBytes_ = 0;
    TrackMetrics      trackMetrics_  = {};

    // ── 任务函数 ──
    static void decoderTaskFunc(void* arg);
    static void writerTaskFunc(void* arg);

    int  refillReadBuf();
    void convertToI2s(const int16_t* pcm, int samples, int channels,
                      int32_t* out, int* outBytes);
    void enablePA();
    void disablePA();
};
