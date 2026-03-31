#pragma once
#include <Arduino.h>
#include <sdmmc_cmd.h>

/**
 * SD 卡读写封装
 * 使用 SDMMC 1-bit 模式，和官方示例保持一致
 * 引脚：CLK=GPIO38, CMD=GPIO21, D0=GPIO39
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
    static constexpr int kPinClk = 38;
    static constexpr int kPinCmd = 21;
    static constexpr int kPinD0  = 39;
    static constexpr const char* kMountPoint = "/sdcard";

    sdmmc_card_t* card_ = nullptr;
    bool          mounted_;
};
