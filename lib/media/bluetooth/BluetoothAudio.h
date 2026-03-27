#pragma once

#include <Arduino.h>
#include "sdkconfig.h"

// 蓝牙 A2DP Sink 封装：接收手机蓝牙音频，通过 I2S/ES8311/PA 播放
//
// 注意：A2DP 属于 Bluetooth Classic，仅原版 ESP32 支持。
// ESP32-S3/C3 等仅支持 BLE，本类在这些平台上提供空实现（编译通过但不可用）。
//
// 使用 stream_reader 回调模式：A2DP 解码后的 PCM 数据通过回调传入，
// 由本类转换格式后写入 I2S（与现有 I2S 驱动共存，不让库自建 I2S）。
//
// I2S 共存策略：
//   begin() → deinitI2s() + initI2s(44100) 切换为 A2DP 标准采样率
//   stop()  → deinitI2s() + initI2s(16000) 恢复给 AudioCodec 使用

#if CONFIG_IDF_TARGET_ESP32  // Bluetooth Classic 仅 ESP32 支持

#include "BluetoothA2DPSink.h"

class BluetoothAudio {
public:
    bool begin(const char* deviceName = "ESP32-BOX-BT");
    void stop();

    bool isConnected() const { return connected_; }
    bool isStarted()   const { return started_; }

    // 平台支持标志
    static constexpr bool kPlatformSupported = true;

private:
    BluetoothA2DPSink a2dpSink_;
    bool              started_   = false;
    static bool       connected_;

    static void enablePA();
    static void disablePA();
    static void audioDataCallback(const uint8_t* data, uint32_t len);
    static void connectionStateCallback(esp_a2d_connection_state_t state, void* obj);
};

#else  // ESP32-S3/C3 等平台：空实现桩

class BluetoothAudio {
public:
    bool begin(const char* deviceName = "ESP32-BOX-BT") {
        Serial.println("[BT] A2DP not supported on this platform (requires ESP32)");
        return false;
    }
    void stop() {}

    bool isConnected() const { return false; }
    bool isStarted()   const { return false; }

    static constexpr bool kPlatformSupported = false;
};

#endif  // CONFIG_IDF_TARGET_ESP32
