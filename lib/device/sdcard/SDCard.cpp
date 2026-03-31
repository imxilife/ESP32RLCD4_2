#include <device/sdcard/SDCard.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>

#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <sys/stat.h>

namespace {
constexpr const char* kSdMountPoint = "/sdcard";

/**功能: 将逻辑路径映射到 SDMMC 的实际挂载点路径 */
std::string resolveSdPath(const char* path) {
    // 空路径和根路径统一映射到挂载根目录。
    if (path == nullptr || path[0] == '\0' || std::strcmp(path, "/") == 0) {
        return kSdMountPoint;
    }

    // 先以挂载点作为完整路径前缀。
    std::string fullPath = kSdMountPoint;
    // 对不带前导斜杠的相对路径补一个目录分隔符。
    if (path[0] != '/') {
        fullPath += '/';
    }
    // 追加调用方传入的逻辑路径。
    fullPath += path;
    // 返回可直接交给 POSIX 文件接口使用的实际路径。
    return fullPath;
}

/**功能: 根据卡的 OCR 信息返回轻量类型描述 */
const char* cardTypeName(const sdmmc_card_t* card) {
    // 卡对象为空时返回兜底类型，避免访问空指针。
    if (card == nullptr) return "UNKNOWN";
    // OCR 的 bit30 为 CCS 位，置位通常表示高容量卡。
    if ((card->ocr & (1UL << 30)) != 0) return "SDHC";
    // 未置位时按标准容量卡处理。
    return "SDSC";
}

/**功能: 使用 POSIX 目录接口递归列出目录内容 */
void listDirRecursive(const std::string& fullPath, uint8_t levels) {
    // 打开当前层目录；失败时直接返回，维持轻量诊断行为。
    DIR* dir = opendir(fullPath.c_str());
    if (dir == nullptr) return;

    // 复用单个目录项指针接收 readdir 结果。
    dirent* entry = nullptr;
    // 逐项遍历目录中的文件和子目录。
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过当前目录和父目录这两个特殊项，避免无意义输出和递归回环。
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 先复制出子项的完整路径，供后续 stat 和递归使用。
        std::string childPath = fullPath;
        // 补上目录分隔符。
        childPath += '/';
        // 拼接当前子项名称。
        childPath += entry->d_name;

        // 用 stat 查询子项类型和大小。
        struct stat st = {};
        // 查询失败时跳过该项，避免单个坏项影响整个目录遍历。
        if (stat(childPath.c_str(), &st) != 0) {
            continue;
        }

        // 目录项单独打印，并在仍允许下钻时继续递归。
        if (S_ISDIR(st.st_mode)) {
            Serial.printf("  DIR : %s\n", entry->d_name);
            if (levels > 0) listDirRecursive(childPath, levels - 1);
        } else {
            // 文件项打印文件名和字节大小，便于排查挂载后的目录结构。
            Serial.printf("  FILE: %s  SIZE: %lld\n",
                          entry->d_name,
                          static_cast<long long>(st.st_size));
        }
    }

    // 遍历结束后关闭目录句柄，释放系统资源。
    closedir(dir);
}
}

/**功能: 构造 SDCard 对象并初始化为未挂载状态 */
SDCard::SDCard()
    // 初始状态下尚未挂载卡。
    : mounted_(false)
{}

/**功能: 按官方 SDMMC 1-bit 方式初始化并挂载 SD 卡
 * 流程:
 * 1. 先准备挂载配置，决定是否格式化、最大打开文件数、分配单元大小等 FAT 层策略。
 * 2. 再准备 SDMMC host 配置，确定由哪套 SDMMC 控制器发起时钟和命令。
 * 3. 然后准备 slot 配置，绑定板级实际使用的 CLK/CMD/D0 引脚，并声明总线位宽。
 * 4. 调用 esp_vfs_fat_sdmmc_mount，让底层完成卡初始化、协议握手、卡信息读取和 FAT 挂载。
 * 5. 挂载成功后保存 card_ 指针，并通过它读取 OCR、容量等信息，供后续诊断和文件操作使用。
 * 6. 之后上层不再直接操作 SDMMC 命令，而是通过 VFS 路径 "/sdcard/..." 访问文件。
 */
bool SDCard::begin() {
    Serial.printf("[SD] SDMMC init: CLK=%d CMD=%d D0=%d mode=1bit\n",
                  kPinClk, kPinCmd, kPinD0);

    esp_vfs_fat_sdmmc_mount_config_t mountConfig = {};
    // 挂载失败时不自动格式化，避免误清卡数据。
    mountConfig.format_if_mount_failed = false;
    // 同时允许打开的文件数保持一个较小值，符合当前轻量使用场景。
    mountConfig.max_files = 5;
    // 沿用官方示例的簇分配单元设置，减少与官方行为的偏差。
    mountConfig.allocation_unit_size = 16 * 1024 * 3;

    // 使用 SDMMC 默认主机配置，保持和官方示例一致。
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // 使用 SDMMC 默认 slot 配置作为基础模板。
    sdmmc_slot_config_t slotConfig = SDMMC_SLOT_CONFIG_DEFAULT();
    // 显式声明 1-bit 宽度，只使用一根数据线。
    slotConfig.width = 1;
    // 绑定板卡实际使用的时钟引脚。
    slotConfig.clk = static_cast<gpio_num_t>(kPinClk);
    // 绑定板卡实际使用的命令引脚。
    slotConfig.cmd = static_cast<gpio_num_t>(kPinCmd);
    // 绑定板卡实际使用的数据线 D0。
    slotConfig.d0 = static_cast<gpio_num_t>(kPinD0);

    // 挂载 FAT 文件系统并拿到卡对象指针，成功后即可走 VFS 文件接口。
    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slotConfig, &mountConfig, &card_);
    Serial.printf("[SD] esp_vfs_fat_sdmmc_mount(...): %s (0x%x)\n",
                  err == ESP_OK ? "OK" : "FAIL", static_cast<unsigned>(err));
    // 挂载失败或卡对象为空都视为初始化失败。
    if (err != ESP_OK || card_ == nullptr) {
        Serial.println("[SD] 挂载失败，未能完成 SDMMC 初始化");
        // 失败时明确保持未挂载状态。
        mounted_ = false;
        // 清空卡对象指针，避免残留无效引用。
        card_ = nullptr;
        // 返回失败，让上层停止后续读写测试。
        return false;
    }

    // 输出底层识别到的卡信息，便于实机排查容量/类型/频率等参数。
    sdmmc_card_print_info(stdout, card_);

    Serial.printf("[SD] OCR: 0x%08lx\n", static_cast<unsigned long>(card_->ocr));
    Serial.printf("[SD] 卡类型: %s\n", cardTypeName(card_));
    Serial.printf("[SD] 容量: %llu MB\n", totalBytes() / (1024 * 1024));

    // 成功走到这里说明卡和文件系统均已可用。
    mounted_ = true;
    // 返回成功给调用方。
    return true;
}

/**功能: 卸载 SD 卡并释放 VFS 与 SDMMC 相关资源 */
void SDCard::end() {
    // 仅在已挂载时执行卸载，避免重复释放。
    if (mounted_) {
        // 解除挂载并释放底层 FAT/SDMMC 资源。
        esp_vfs_fat_sdcard_unmount(kMountPoint, card_);
        // 清空卡对象指针，标记资源不再有效。
        card_ = nullptr;
        // 回到未挂载状态。
        mounted_ = false;
    }
}

/**功能: 返回当前 SD 卡是否已经成功挂载 */
bool SDCard::isMounted() const { return mounted_; }

/**功能: 根据卡的 CSD 信息返回总容量字节数 */
uint64_t SDCard::totalBytes() const {
    // 未挂载或没有卡对象时直接返回 0。
    if (!mounted_ || card_ == nullptr) return 0;
    // CSD 里的容量和扇区大小足够推导整卡总字节数。
    return static_cast<uint64_t>(card_->csd.capacity) * card_->csd.sector_size;
}

/**功能: 根据 FATFS 空闲簇信息估算已使用容量 */
uint64_t SDCard::usedBytes() const {
    // 未挂载时没有可用卷信息，直接返回 0。
    if (!mounted_) return 0;

    // 接收 FATFS 卷信息和空闲簇数量。
    FATFS* fsinfo = nullptr;
    DWORD freeClusters = 0;
    // 读取失败或卷信息为空时返回 0，避免使用无效数据。
    if (f_getfree("0:", &freeClusters, &fsinfo) != 0 || fsinfo == nullptr) return 0;

#if _MAX_SS != 512
    // 若工程启用了可变扇区大小，则使用卷中的实际扇区大小。
    const uint64_t sectorSize = fsinfo->ssize;
#else
    // 否则沿用 FATFS 默认的 512 字节扇区大小。
    const uint64_t sectorSize = 512;
#endif
    // csize 是每簇扇区数，n_fatent-2 为总数据簇，free_clst 为剩余空闲簇。
    return static_cast<uint64_t>(fsinfo->csize) *
           ((fsinfo->n_fatent - 2) - fsinfo->free_clst) *
           sectorSize;
}

/**功能: 以覆盖方式写入文本文件 */
bool SDCard::writeFile(const char* path, const char* content) {
    // 未挂载时禁止文件操作。
    if (!mounted_) return false;

    // 将逻辑路径映射为真实挂载路径。
    const std::string fullPath = resolveSdPath(path);
    // 以覆盖写模式打开文件，不存在则创建。
    FILE* file = fopen(fullPath.c_str(), "wb");
    if (file == nullptr) { Serial.printf("[SD] 写入失败: %s\n", path); return false; }

    // 计算待写入字符串长度。
    const size_t len = std::strlen(content);
    // 一次性把字符串写入文件。
    const size_t written = fwrite(content, 1, len, file);
    // 无论写入是否完整都关闭文件句柄。
    fclose(file);
    // 仅当写入字节数完全一致时认为成功。
    return written == len;
}

/**功能: 以追加方式写入文本文件 */
bool SDCard::appendFile(const char* path, const char* content) {
    // 未挂载时禁止文件操作。
    if (!mounted_) return false;

    // 将逻辑路径映射为真实挂载路径。
    const std::string fullPath = resolveSdPath(path);
    // 以追加模式打开文件，写入内容会追加到尾部。
    FILE* file = fopen(fullPath.c_str(), "ab");
    if (file == nullptr) return false;

    // 计算待追加字符串长度。
    const size_t len = std::strlen(content);
    // 将全部字节写到文件尾部。
    const size_t written = fwrite(content, 1, len, file);
    // 写完后及时关闭文件句柄。
    fclose(file);
    // 只有全部字节都成功写入才返回成功。
    return written == len;
}

/**功能: 读取整个文本文件内容到 String */
bool SDCard::readFile(const char* path, String& out) {
    // 未挂载时禁止文件操作。
    if (!mounted_) return false;

    // 将逻辑路径映射为真实挂载路径。
    const std::string fullPath = resolveSdPath(path);
    // 以只读模式打开文件。
    FILE* file = fopen(fullPath.c_str(), "rb");
    if (file == nullptr) { Serial.printf("[SD] 读取失败: %s\n", path); return false; }

    // 先定位到文件尾部，准备计算完整文件大小。
    if (fseek(file, 0, SEEK_END) != 0) {
        // 定位失败时关闭文件并返回失败。
        fclose(file);
        return false;
    }

    // 读取当前文件指针位置作为文件总长度。
    const long fileSize = ftell(file);
    // 若长度非法则直接失败。
    if (fileSize < 0) {
        fclose(file);
        return false;
    }
    // 回到文件头，准备顺序读取文件内容。
    rewind(file);

    // 预留足够空间，减少后续 String 扩容次数。
    out.reserve(fileSize);
    // 清空输出字符串，确保结果只包含本次读取内容。
    out = "";
    // 以固定大小缓冲区循环读取，直到到达文件末尾。
    while (!feof(file)) {
        // 临时缓冲区用于承接每一块读取结果。
        char buf[128];
        // 从文件读取一块数据。
        const size_t bytesRead = fread(buf, 1, sizeof(buf), file);
        // 读取到有效字节时追加到输出字符串。
        if (bytesRead > 0) out.concat(buf, bytesRead);
    }
    // 读取完成后关闭文件句柄。
    fclose(file);
    // 返回成功。
    return true;
}

/**功能: 删除指定文件 */
bool SDCard::removeFile(const char* path) {
    // 未挂载时禁止文件操作。
    if (!mounted_) return false;
    // 将逻辑路径映射为真实挂载路径。
    const std::string fullPath = resolveSdPath(path);
    // 直接调用 POSIX remove 删除目标文件。
    return std::remove(fullPath.c_str()) == 0;
}

/**功能: 判断指定路径是否存在 */
bool SDCard::exists(const char* path) {
    // 未挂载时直接认为路径不存在。
    if (!mounted_) return false;
    // 将逻辑路径映射为真实挂载路径。
    const std::string fullPath = resolveSdPath(path);
    // 通过 stat 查询路径元信息。
    struct stat st = {};
    // 查询成功即表示目标存在。
    return stat(fullPath.c_str(), &st) == 0;
}

/**功能: 列出指定目录内容并按层级递归遍历 */
void SDCard::listDir(const char* dirname, uint8_t levels) {
    // 未挂载时不做任何目录访问。
    if (!mounted_) return;
    // 把逻辑目录路径映射后交给递归遍历函数处理。
    listDirRecursive(resolveSdPath(dirname), levels);
}
