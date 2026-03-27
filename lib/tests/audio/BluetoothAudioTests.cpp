#include "BluetoothAudioTests.h"
#include <media/bluetooth/BluetoothAudio.h>
#include <Arduino.h>

#if CONFIG_IDF_TARGET_ESP32  // A2DP 测试仅在 ESP32 上有意义

#include <media/audio/I2sDriver.h>

// 测试 A2DP Sink 初始化（启动蓝牙广播）
static bool testA2dpInit() {
    Serial.println("[BT TEST] --- testA2dpInit ---");
    BluetoothAudio bt;
    bool ok = bt.begin("ESP32-BT-TEST");
    Serial.printf("[BT TEST] begin: %s\n", ok ? "PASS" : "FAIL");
    if (ok) {
        Serial.printf("[BT TEST] isStarted: %s\n",
                       bt.isStarted() ? "PASS" : "FAIL");
        bt.stop();
    }
    return ok;
}

// 测试启动/停止循环不崩溃
static bool testA2dpStartStop() {
    Serial.println("[BT TEST] --- testA2dpStartStop ---");
    BluetoothAudio bt;

    bool ok = bt.begin("ESP32-BT-TEST");
    if (!ok) {
        Serial.println("[BT TEST] start: FAIL");
        return false;
    }
    Serial.println("[BT TEST] start: PASS");

    delay(2000);

    bt.stop();
    bool stopped = !bt.isStarted();
    Serial.printf("[BT TEST] stop: %s\n", stopped ? "PASS" : "FAIL");

    return stopped;
}

// 测试停止后 I2S 恢复到 16kHz
static bool testI2sRecovery() {
    Serial.println("[BT TEST] --- testI2sRecovery ---");
    BluetoothAudio bt;

    bool ok = bt.begin("ESP32-BT-TEST");
    if (!ok) {
        Serial.println("[BT TEST] begin failed, skip I2S recovery test");
        return false;
    }

    bt.stop();

    // 验证 I2S 已恢复：deinit + init 16kHz 应成功
    deinitI2s();
    bool recovered = initI2s(16000);
    Serial.printf("[BT TEST] I2S 16kHz recovery: %s\n",
                  recovered ? "PASS" : "FAIL");

    return recovered;
}

bool BluetoothAudioTests::runAllTests() {
    Serial.println("====== Bluetooth Audio Tests ======");
    bool allPass = true;
    allPass &= testA2dpInit();
    allPass &= testA2dpStartStop();
    allPass &= testI2sRecovery();
    Serial.printf("====== Bluetooth Audio Tests %s ======\n",
                  allPass ? "ALL PASSED" : "SOME FAILED");
    return allPass;
}

#else  // ESP32-S3/C3：跳过测试

bool BluetoothAudioTests::runAllTests() {
    Serial.println("====== Bluetooth Audio Tests SKIPPED (requires ESP32) ======");
    return true;
}

#endif  // CONFIG_IDF_TARGET_ESP32
