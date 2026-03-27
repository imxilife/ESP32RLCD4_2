#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

/**
 * SD 卡读写封装
 * 使用 SPI2_HOST (HSPI)，与 Display 的 SPI3_HOST 互不干扰
 * 引脚：MOSI=GPIO21, SCK=GPIO38, MISO=GPIO39, CS=GPIO17
 */
class SDCard {
public:
    SDCard();

    bool begin();
    void end();
    bool isMounted() const;

    uint64_t totalBytes() const;
    uint64_t usedBytes() const;

    bool writeFile(const char* path, const char* content);
    bool appendFile(const char* path, const char* content);
    bool readFile(const char* path, String& out);
    bool removeFile(const char* path);
    bool exists(const char* path);
    void listDir(const char* dirname, uint8_t levels = 0);

private:
    static constexpr int kPinMOSI = 21;
    static constexpr int kPinSCK  = 38;
    static constexpr int kPinMISO = 39;
    static constexpr int kPinCS   = 17;

    SPIClass spi_;
    bool     mounted_;
};
