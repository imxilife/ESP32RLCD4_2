#include <device/sdcard/SDCard.h>

SDCard::SDCard()
    : spi_(HSPI),
      mounted_(false)
{}

bool SDCard::begin() {
    spi_.begin(kPinSCK, kPinMISO, kPinMOSI, kPinCS);

    if (!SD.begin(kPinCS, spi_, 4000000)) {
        Serial.println("[SD] 挂载失败");
        mounted_ = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] 未检测到卡");
        mounted_ = false;
        return false;
    }

    Serial.printf("[SD] 卡类型: %s\n",
        cardType == CARD_MMC  ? "MMC"  :
        cardType == CARD_SD   ? "SDSC" :
        cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    Serial.printf("[SD] 容量: %llu MB\n", SD.totalBytes() / (1024 * 1024));

    mounted_ = true;
    return true;
}

void SDCard::end() {
    if (mounted_) {
        SD.end();
        spi_.end();
        mounted_ = false;
    }
}

bool SDCard::isMounted() const { return mounted_; }
uint64_t SDCard::totalBytes() const { return mounted_ ? SD.totalBytes() : 0; }
uint64_t SDCard::usedBytes()  const { return mounted_ ? SD.usedBytes()  : 0; }

bool SDCard::writeFile(const char* path, const char* content) {
    if (!mounted_) return false;
    File f = SD.open(path, FILE_WRITE);
    if (!f) { Serial.printf("[SD] 写入失败: %s\n", path); return false; }
    bool ok = f.print(content);
    f.close();
    return ok;
}

bool SDCard::appendFile(const char* path, const char* content) {
    if (!mounted_) return false;
    File f = SD.open(path, FILE_APPEND);
    if (!f) return false;
    bool ok = f.print(content);
    f.close();
    return ok;
}

bool SDCard::readFile(const char* path, String& out) {
    if (!mounted_) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) { Serial.printf("[SD] 读取失败: %s\n", path); return false; }
    out = f.readString();
    f.close();
    return true;
}

bool SDCard::removeFile(const char* path) {
    if (!mounted_) return false;
    return SD.remove(path);
}

bool SDCard::exists(const char* path) {
    if (!mounted_) return false;
    return SD.exists(path);
}

void SDCard::listDir(const char* dirname, uint8_t levels) {
    if (!mounted_) return;
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  DIR : %s\n", file.name());
            if (levels) listDir(file.path(), levels - 1);
        } else {
            Serial.printf("  FILE: %s  SIZE: %d\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}
