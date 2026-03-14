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
