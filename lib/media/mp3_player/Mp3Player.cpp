#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "Mp3Player.h"
#include <SD.h>
#include <media/audio/Es8311Init.h>
#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

#define MP3_I2S_PORT  I2S_NUM_0
#define PA_PIN        46

struct Mp3Player::Impl {
    File file;
};

Mp3Player::Mp3Player()
    : impl_(new Impl()) {
}

Mp3Player::~Mp3Player() {
    if (impl_ != nullptr) {
        if (impl_->file) impl_->file.close();
        delete impl_;
        impl_ = nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════
//  begin() —— 分配双缓冲和信号量
// ═══════════════════════════════════════════════════════════════
bool Mp3Player::begin() {
    for (int i = 0; i < 2; i++) {
        pcmBuf_[i] = (int32_t*)heap_caps_malloc(kPcmBufBytes, MALLOC_CAP_SPIRAM);
        if (!pcmBuf_[i]) {
            Serial.println("[MP3] PCM buf alloc failed");
            return false;
        }
        bufReady_[i] = xSemaphoreCreateBinary();
        bufFree_[i]  = xSemaphoreCreateBinary();
        xSemaphoreGive(bufFree_[i]);  // 初始状态：两个缓冲区都空闲
    }

    readBuf_ = (uint8_t*)heap_caps_malloc(kReadBufSize, MALLOC_CAP_SPIRAM);
    if (!readBuf_) {
        Serial.println("[MP3] Read buf alloc failed");
        return false;
    }

    Serial.println("[MP3] Player ready");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  play() —— 启动双任务播放 MP3
// ═══════════════════════════════════════════════════════════════
bool Mp3Player::play(const char* path) {
    if (playing_) return false;

    impl_->file = SD.open(path, FILE_READ);
    if (!impl_->file) {
        Serial.printf("[MP3] Cannot open: %s\n", path);
        return false;
    }

    Serial.printf("[MP3] Playing: %s (%d bytes)\n", path, impl_->file.size());

    stopRequested_ = false;
    playing_ = true;
    readBufValid_ = 0;
    readBufPos_ = 0;
    pcmBufLen_[0] = 0;
    pcmBufLen_[1] = 0;

    // 重置信号量状态
    for (int i = 0; i < 2; i++) {
        xSemaphoreTake(bufReady_[i], 0);
        xSemaphoreTake(bufFree_[i], 0);
        xSemaphoreGive(bufFree_[i]);
    }

    xTaskCreatePinnedToCore(decoderTaskFunc, "Mp3Decoder",
                            8192, this, 5, &decoderTask_, 1);
    xTaskCreatePinnedToCore(writerTaskFunc, "Mp3Writer",
                            4096, this, 6, &writerTask_, 1);
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  stop() —— 停止播放，等待任务退出
// ═══════════════════════════════════════════════════════════════
void Mp3Player::stop() {
    if (!playing_) return;
    stopRequested_ = true;

    // 解锁可能阻塞在信号量上的任务
    xSemaphoreGive(bufFree_[0]);
    xSemaphoreGive(bufFree_[1]);
    xSemaphoreGive(bufReady_[0]);
    xSemaphoreGive(bufReady_[1]);

    for (int i = 0; i < 200 && playing_; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    decoderTask_ = nullptr;
    writerTask_  = nullptr;
}

// ═══════════════════════════════════════════════════════════════
//  scanMp3Files() —— 扫描目录下的 .mp3 文件
// ═══════════════════════════════════════════════════════════════
int Mp3Player::scanMp3Files(const char* dir, String names[], int maxCount) {
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) return 0;

    int count = 0;
    File entry = root.openNextFile();
    while (entry && count < maxCount) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
                // 构建完整路径
                names[count] = String(dir) + "/" + name;
                Serial.printf("[MP3] Found: %s\n", names[count].c_str());
                count++;
            }
        }
        entry = root.openNextFile();
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════
//  PA 控制 —— 复用 AudioCodec 的同一套时序
// ═══════════════════════════════════════════════════════════════
void Mp3Player::enablePA() {
    i2s_zero_dma_buffer(MP3_I2S_PORT);
    es8311SetDacVolume(0xBF);       // 0 dB
    digitalWrite(PA_PIN, HIGH);
}

void Mp3Player::disablePA() {
    vTaskDelay(pdMS_TO_TICKS(100)); // 等 DMA 播完
    digitalWrite(PA_PIN, LOW);
    es8311SetDacVolume(0x00);
    i2s_zero_dma_buffer(MP3_I2S_PORT);
}

// ═══════════════════════════════════════════════════════════════
//  refillReadBuf() —— 从 SD 补充 MP3 压缩数据
// ═══════════════════════════════════════════════════════════════
//  将未消耗数据 memmove 到缓冲区头部，再从文件补充
int Mp3Player::refillReadBuf() {
    int remaining = readBufValid_ - readBufPos_;
    if (remaining > 0 && readBufPos_ > 0) {
        memmove(readBuf_, readBuf_ + readBufPos_, remaining);
    }
    readBufValid_ = remaining;
    readBufPos_ = 0;

    int space = kReadBufSize - readBufValid_;
    if (space > 0 && impl_->file.available()) {
        int bytesRead = impl_->file.read(readBuf_ + readBufValid_, space);
        if (bytesRead > 0) readBufValid_ += bytesRead;
        return bytesRead;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  convertToI2s() —— PCM int16 → I2S int32 格式转换
// ═══════════════════════════════════════════════════════════════
//  I2S 格式：32-bit stereo，16-bit PCM 左对齐 (sample << 16)
//  Mono MP3 → 复制到左右声道；Stereo → 直接映射
void Mp3Player::convertToI2s(const int16_t* pcm, int samples, int channels,
                              int32_t* out, int* outBytes) {
    int idx = 0;
    if (channels == 1) {
        for (int i = 0; i < samples; i++) {
            int32_t s = ((int32_t)pcm[i]) << 16;
            out[idx++] = s;  // L
            out[idx++] = s;  // R
        }
    } else {
        for (int i = 0; i < samples * 2; i++) {
            out[idx++] = ((int32_t)pcm[i]) << 16;
        }
    }
    *outBytes = idx * (int)sizeof(int32_t);
}

// ═══════════════════════════════════════════════════════════════
//  decoderTaskFunc() —— 解码器任务 (producer)
// ═══════════════════════════════════════════════════════════════
//  读 SD → minimp3 解码 → 填充双缓冲中的一个 → 信号通知 writer
void Mp3Player::decoderTaskFunc(void* arg) {
    Mp3Player* self = static_cast<Mp3Player*>(arg);

    mp3dec_t dec;
    mp3dec_init(&dec);

    // minimp3 解码输出的临时缓冲区 (最大 1152 samples × 2 channels)
    int16_t pcmTemp[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t info;

    bool firstFrame = true;
    int  activeBuf = 0;
    int  errorCount = 0;

    // 初始填充读取缓冲区
    self->refillReadBuf();

    while (!self->stopRequested_) {
        int available = self->readBufValid_ - self->readBufPos_;

        // 数据不足时尝试补充
        if (available < 512) {
            int added = self->refillReadBuf();
            available = self->readBufValid_ - self->readBufPos_;
            if (available == 0 && added == 0) break;  // EOF
        }

        // 等待缓冲区可写
        if (xSemaphoreTake(self->bufFree_[activeBuf],
                           pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;  // 超时重试（检查 stopRequested_）
        }
        if (self->stopRequested_) break;

        // 解码一帧
        int samples = mp3dec_decode_frame(&dec,
            self->readBuf_ + self->readBufPos_, available,
            pcmTemp, &info);

        if (info.frame_bytes > 0) {
            self->readBufPos_ += info.frame_bytes;
            errorCount = 0;
        } else if (info.frame_bytes == 0) {
            // 无法识别帧，可能是 ID3 tag 或损坏数据
            // 跳过 1 字节继续搜索同步头
            if (available > 0) {
                self->readBufPos_++;
                errorCount++;
                if (errorCount > 1024) {
                    Serial.println("[MP3] Too many errors, abort");
                    break;
                }
            }
            xSemaphoreGive(self->bufFree_[activeBuf]);  // 归还缓冲区
            continue;
        }

        if (samples == 0) {
            // 帧被消耗但无输出（如 ID3 帧），继续
            xSemaphoreGive(self->bufFree_[activeBuf]);
            continue;
        }

        // 首帧：配置 I2S 采样率
        if (firstFrame && info.hz > 0) {
            firstFrame = false;
            Serial.printf("[MP3] %d Hz, %d ch, %d kbps\n",
                          info.hz, info.channels, info.bitrate_kbps);
            i2s_set_clk(MP3_I2S_PORT, info.hz,
                        I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
        }

        // PCM → I2S 格式转换，写入双缓冲
        self->convertToI2s(pcmTemp, samples, info.channels,
                           self->pcmBuf_[activeBuf],
                           &self->pcmBufLen_[activeBuf]);

        // 通知 writer 缓冲区已就绪
        xSemaphoreGive(self->bufReady_[activeBuf]);
        activeBuf ^= 1;  // 切换到另一个缓冲区
    }

    // 发送 EOF sentinel
    for (int i = 0; i < 2; i++) {
        xSemaphoreTake(self->bufFree_[i], pdMS_TO_TICKS(200));
        self->pcmBufLen_[i] = -1;
        xSemaphoreGive(self->bufReady_[i]);
    }

    if (self->impl_->file) self->impl_->file.close();
    Serial.println("[MP3] Decoder done");
    vTaskDelete(nullptr);
}

// ═══════════════════════════════════════════════════════════════
//  writerTaskFunc() —— 写入任务 (consumer)
// ═══════════════════════════════════════════════════════════════
//  从双缓冲读取 PCM → i2s_write() → ES8311 → PA → Speaker
void Mp3Player::writerTaskFunc(void* arg) {
    Mp3Player* self = static_cast<Mp3Player*>(arg);

    // PA 启动序列
    self->enablePA();

    int consumerBuf = 0;

    while (!self->stopRequested_) {
        // 等待 decoder 填充缓冲区
        if (xSemaphoreTake(self->bufReady_[consumerBuf],
                           pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;  // 超时重试
        }
        if (self->stopRequested_) break;

        int len = self->pcmBufLen_[consumerBuf];
        if (len <= 0) break;  // EOF sentinel

        // 分块写入 I2S
        uint8_t* data = (uint8_t*)self->pcmBuf_[consumerBuf];
        int written = 0;
        while (written < len && !self->stopRequested_) {
            size_t bytesOut = 0;
            int want = len - written;
            if (want > 512) want = 512;
            i2s_write(MP3_I2S_PORT, data + written, want,
                      &bytesOut, pdMS_TO_TICKS(50));
            written += (int)bytesOut;
        }

        // 归还缓冲区给 decoder
        xSemaphoreGive(self->bufFree_[consumerBuf]);
        consumerBuf ^= 1;
    }

    // PA 关闭序列
    self->disablePA();

    // 恢复原始采样率 (16kHz for record-playback)
    i2s_set_clk(MP3_I2S_PORT, 16000,
                I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);

    self->playing_ = false;
    Serial.println("[MP3] Writer done");
    vTaskDelete(nullptr);
}
