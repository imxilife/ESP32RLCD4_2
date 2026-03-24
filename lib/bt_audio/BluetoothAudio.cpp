/**
 * @file BluetoothAudio.cpp
 * @brief A2DP Sink 实现 —— 接收手机蓝牙音频并写入 I2S/ES8311
 *
 * 仅在原版 ESP32 上编译（Bluetooth Classic 硬件支持）。
 * ESP32-S3/C3 上由头文件中的 inline 空实现替代。
 *
 * 数据流：
 *   手机 → BT A2DP(SBC 解码) → audioDataCallback()
 *     → int16 stereo PCM 转 int32 I2S 格式
 *     → i2s_write(I2S_NUM_0) → ES8311(DAC) → PA(GPIO46) → Speaker
 *
 * I2S 共存：
 *   A2DP 标准输出为 44100Hz 16-bit stereo。
 *   begin() 时重建 I2S 驱动为 44100Hz，stop() 时恢复 16kHz。
 */

#include "BluetoothAudio.h"

#if CONFIG_IDF_TARGET_ESP32  // 仅 ESP32 编译以下实现

#include "I2sDriver.h"
#include "Es8311Init.h"
#include "driver/i2s.h"

#define AUDIO_I2S_PORT I2S_NUM_0
#define PA_PIN         46

bool BluetoothAudio::connected_ = false;

bool BluetoothAudio::begin(const char* deviceName) {
    if (started_) return true;

    // 切换 I2S 采样率：卸载 16kHz → 安装 44100Hz
    deinitI2s();
    if (!initI2s(44100)) {
        Serial.println("[BT] I2S 44100Hz init failed, fallback to 16kHz");
        initI2s(16000);
        return false;
    }
    // 重新初始化 ES8311 匹配新采样率
    initEs8311(44100);

    // 清空 DMA 残留，避免开 PA 时有噪声
    i2s_zero_dma_buffer(AUDIO_I2S_PORT);

    // 配置 A2DP Sink：使用 stream_reader 回调，不让库自建 I2S
    a2dpSink_.set_stream_reader(audioDataCallback, false);
    a2dpSink_.set_on_connection_state_changed(connectionStateCallback, this);
    a2dpSink_.set_auto_reconnect(true);

    // 启动蓝牙广播
    a2dpSink_.start(deviceName);
    started_ = true;

    Serial.printf("[BT] A2DP Sink started, name: %s\n", deviceName);
    return true;
}

void BluetoothAudio::stop() {
    if (!started_) return;

    // 安全关闭音频输出
    disablePA();
    es8311SetDacVolume(0x00);
    i2s_zero_dma_buffer(AUDIO_I2S_PORT);

    // 停止 A2DP（true = 断开连接并释放资源）
    a2dpSink_.end(true);
    started_   = false;
    connected_ = false;

    // 恢复 I2S 到 16kHz（供 AudioCodec / Mp3Player 使用）
    deinitI2s();
    initI2s(16000);
    initEs8311(16000);

    Serial.println("[BT] A2DP Sink stopped, I2S restored to 16kHz");
}

void BluetoothAudio::enablePA() {
    // 先设 DAC 音量再开 PA，避免 PA 开启瞬间放大噪声
    es8311SetDacVolume(0xBF);   // 0dB
    digitalWrite(PA_PIN, HIGH);
}

void BluetoothAudio::disablePA() {
    // 先关 PA 再静音 DAC，避免 pop 噪声
    digitalWrite(PA_PIN, LOW);
    es8311SetDacVolume(0x00);
}

// A2DP 音频数据回调（在蓝牙任务上下文中调用）
// data: int16_t stereo PCM (L,R,L,R,...), 44100Hz
// len:  字节数
void BluetoothAudio::audioDataCallback(const uint8_t* data, uint32_t len) {
    if (len == 0) return;

    const int16_t* pcm16   = reinterpret_cast<const int16_t*>(data);
    int            samples = len / sizeof(int16_t);

    // 转换为 int32 I2S 格式：每个 int16 样本左移 16 位放入 32-bit slot 高位
    // 使用栈上小缓冲区分批写入，避免大块 PSRAM 分配
    static constexpr int kBatchSamples = 256;
    int32_t i2sBuf[kBatchSamples];

    int offset = 0;
    while (offset < samples) {
        int batch = samples - offset;
        if (batch > kBatchSamples) batch = kBatchSamples;

        for (int i = 0; i < batch; i++) {
            i2sBuf[i] = ((int32_t)pcm16[offset + i]) << 16;
        }

        size_t bytesWritten = 0;
        i2s_write(AUDIO_I2S_PORT, i2sBuf, batch * sizeof(int32_t),
                  &bytesWritten, pdMS_TO_TICKS(50));
        offset += batch;
    }
}

// 蓝牙连接状态回调
void BluetoothAudio::connectionStateCallback(esp_a2d_connection_state_t state, void* obj) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            connected_ = true;
            enablePA();
            Serial.println("[BT] Connected");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            connected_ = false;
            disablePA();
            Serial.println("[BT] Disconnected");
            break;
        default:
            break;
    }
}

#endif  // CONFIG_IDF_TARGET_ESP32
