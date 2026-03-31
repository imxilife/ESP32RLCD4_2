#include "SDCardTests.h"
#include <device/sdcard/SDCard.h>

bool SDCardTests::testDetect() {
    Serial.println("[SD TEST] --- Phase 1: Detect Card ---");
    SDCard sd;
    const bool ok = sd.begin();
    Serial.printf("[SD TEST] detect: %s\n", ok ? "PASS" : "FAIL");
    if (!ok) {
        Serial.println("[SD TEST] detect failed, card not ready");
        return false;
    }

    Serial.printf("[SD TEST] totalBytes: %llu MB\n", sd.totalBytes() / (1024 * 1024));
    sd.end();
    return true;
}

bool SDCardTests::testReadWrite() {
    Serial.println("[SD TEST] --- Phase 2: Read/Write Validation ---");
    SDCard sd;
    if (!sd.begin()) {
        Serial.println("[SD TEST] phase 2 aborted, detect failed");
        return false;
    }

    const char* path = "/sd_test_tmp.txt";
    const char* content = "Hello SD Card from ESP32-S3!";
    bool allPass = true;

    bool ok = sd.writeFile(path, content);
    Serial.printf("[SD TEST] write: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    String readBack;
    ok = sd.readFile(path, readBack);
    Serial.printf("[SD TEST] read: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    bool match = (readBack == content);
    Serial.printf("[SD TEST] content match: %s\n", match ? "PASS" : "FAIL");
    allPass &= match;

    const char* extra = "\nAppended line";
    ok = sd.appendFile(path, extra);
    Serial.printf("[SD TEST] append: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    String readBack2;
    ok = sd.readFile(path, readBack2);
    String expected = String(content) + extra;
    bool matchAppend = (readBack2 == expected);
    Serial.printf("[SD TEST] append verify: %s\n", matchAppend ? "PASS" : "FAIL");
    allPass &= matchAppend;

    ok = sd.removeFile(path);
    Serial.printf("[SD TEST] delete: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    bool gone = !sd.exists(path);
    Serial.printf("[SD TEST] verify deleted: %s\n", gone ? "PASS" : "FAIL");
    allPass &= gone;

    Serial.println("[SD TEST] listDir /:");
    sd.listDir("/", 1);

    sd.end();
    return allPass;
}

bool SDCardTests::runAllTests() {
    Serial.println("====== SD Card Tests ======");
    const bool detectOk = testDetect();
    if (!detectOk) {
        Serial.println("[SD TEST] detect failed, skip read/write phase");
        Serial.println("====== SD Card Tests DETECT FAILED ======");
        return false;
    }

    const bool rwOk = testReadWrite();
    Serial.printf("====== SD Card Tests %s ======\n",
                  rwOk ? "ALL PASSED" : "READ/WRITE FAILED");
    return rwOk;
}
