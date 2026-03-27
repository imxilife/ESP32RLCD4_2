#include "SDCardTests.h"
#include <device/sdcard/SDCard.h>

static bool testMountUnmount() {
    Serial.println("[SD TEST] --- testMountUnmount ---");
    SDCard sd;
    bool ok = sd.begin();
    Serial.printf("[SD TEST] mount: %s\n", ok ? "PASS" : "FAIL");
    if (!ok) return false;
    Serial.printf("[SD TEST] totalBytes: %llu MB\n", sd.totalBytes() / (1024 * 1024));
    sd.end();
    return true;
}

static bool testWriteReadDelete() {
    Serial.println("[SD TEST] --- testWriteReadDelete ---");
    SDCard sd;
    if (!sd.begin()) return false;

    const char* path = "/sd_test_tmp.txt";
    const char* content = "Hello SD Card from ESP32-S3!";
    bool allPass = true;

    // Write
    bool ok = sd.writeFile(path, content);
    Serial.printf("[SD TEST] write: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    // Read back
    String readBack;
    ok = sd.readFile(path, readBack);
    Serial.printf("[SD TEST] read: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    // Verify content
    bool match = (readBack == content);
    Serial.printf("[SD TEST] content match: %s\n", match ? "PASS" : "FAIL");
    allPass &= match;

    // Append
    const char* extra = "\nAppended line";
    ok = sd.appendFile(path, extra);
    Serial.printf("[SD TEST] append: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    // Read after append
    String readBack2;
    ok = sd.readFile(path, readBack2);
    String expected = String(content) + extra;
    bool matchAppend = (readBack2 == expected);
    Serial.printf("[SD TEST] append verify: %s\n", matchAppend ? "PASS" : "FAIL");
    allPass &= matchAppend;

    // Delete
    ok = sd.removeFile(path);
    Serial.printf("[SD TEST] delete: %s\n", ok ? "PASS" : "FAIL");
    allPass &= ok;

    // Verify deleted
    bool gone = !sd.exists(path);
    Serial.printf("[SD TEST] verify deleted: %s\n", gone ? "PASS" : "FAIL");
    allPass &= gone;

    sd.end();
    return allPass;
}

static bool testListDir() {
    Serial.println("[SD TEST] --- testListDir ---");
    SDCard sd;
    if (!sd.begin()) return false;
    Serial.println("[SD TEST] listDir /:");
    sd.listDir("/", 1);
    sd.end();
    return true;
}

bool SDCardTests::runAllTests() {
    Serial.println("====== SD Card Tests ======");
    bool allPass = true;
    allPass &= testMountUnmount();
    allPass &= testWriteReadDelete();
    allPass &= testListDir();
    Serial.printf("====== SD Card Tests %s ======\n",
                  allPass ? "ALL PASSED" : "SOME FAILED");
    return allPass;
}
