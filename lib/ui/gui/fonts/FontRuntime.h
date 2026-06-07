#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

/**
 * FontRuntime
 *
 * 类的作用：
 * - FontRuntime 表示“一个字体已经被加载后的运行态资源和状态”。
 * - 它负责保存 SPIFFS 文件句柄、header 元数据、PSRAM 索引缓存、单字形 bitmap 缓存和最近错误。
 * - 它不负责决定加载哪个字体、不负责 fallback、不负责 LRU 淘汰，也不解析 F24BIN1 的业务含义；
 *   这些分别由 FontManager 和 FontBinLoader 完成。
 *
 * 字体加载到使用的流程：
 * step1: FontManager 发现目标字体未加载，调用 FontBinLoader::load()。
 * step2: FontBinLoader 调用 FontRuntime::open() 打开 SPIFFS 字体文件。
 * step3: FontBinLoader 读取并校验 F24BIN1 header，通过 FontRuntime::configure() 保存元数据。
 * step4: 如配置允许，FontRuntime::allocateIndexCache() 在 PSRAM 中申请索引缓存。
 * step5: FontBinLoader 将索引解码到 FontRuntime 的 PSRAM 缓存，随后 markReady() 标记可用。
 * step6: Gui 查字形时，FontBinLoader::getGlyph() 通过 FontRuntime 的元数据和缓存定位字形。
 * step7: getGlyph() 将单个 bitmap 读入 FontRuntime::glyphCache()，返回给 Gui 绘制。
 * step8: FontManager 的 LRU 或 end() 调用 FontBinLoader::unload()，最终由 reset() 关闭文件并释放 PSRAM。
 *
 * 资源约束：
 * - file_ 在 open() 成功后保持打开，reset()/析构时关闭，避免每个字形都重新打开文件。
 * - indexEntries_ 只在 PSRAM 缓存成功时存在，reset()/releaseIndexCache() 释放。
 * - glyphCache_ 是固定 1024 字节小缓存，每次 getGlyph() 覆盖，不做小块动态分配。
 * - lastError_ 只保存静态字符串指针，不在运行时分配堆内存。
 */
class FontRuntime {
public:
    /**
     * 单条字形索引的运行时结构。
     *
     * 作用：
     * - 保存从 F24BIN1 16 字节 raw index entry 解码后的字段。
     * - PSRAM 索引缓存中保存的就是该结构数组，避免每次查找都重复解码。
     */
    struct GlyphEntry {
        uint32_t codepoint;
        uint32_t bitmapOffset;
        uint16_t width;
        uint16_t height;
        uint16_t strideBytes;
        uint16_t advanceX;
    };

    static constexpr size_t kGlyphCacheSize = 1024;

    /**
     * 构造空运行态。
     *
     * 调用时机：
     * - FontManager 创建 FontSlot 时自动构造。
     *
     * 行为：
     * - 不打开文件、不分配 PSRAM，只把状态置为 not loaded。
     */
    FontRuntime();

    /**
     * 析构运行态。
     *
     * 行为：
     * - 关闭仍然打开的文件句柄，释放仍然存在的 PSRAM 索引缓存。
     */
    ~FontRuntime();

    FontRuntime(const FontRuntime&) = delete;
    FontRuntime& operator=(const FontRuntime&) = delete;

    /**
     * 重置运行态并释放所有资源。
     *
     * 调用时机：
     * - FontBinLoader::load() 重新加载前、FontManager LRU 卸载、FontManager::end()、析构。
     *
     * 参数：
     * - error: 重置后保留的状态说明；为空时写入 "not loaded"。
     *
     * 失败行为：
     * - 可重复调用；即使当前未加载也安全。
     */
    void reset(const char* error = "not loaded");

    /**
     * 打开 SPIFFS 中的字体文件。
     *
     * 调用时机：
     * - FontBinLoader::load() 的第一步。
     *
     * 返回值：
     * - true: 文件存在、可打开、不是目录；fileSize() 可用于后续 header 校验。
     * - false: 路径为空、文件不存在、打开失败或路径是目录；lastError() 保存原因。
     *
     * 注意：
     * - 这里只做文件层校验，不校验 F24BIN1 header；格式校验属于 FontBinLoader。
     */
    bool open(fs::FS& fs, const char* path);

    /**
     * 保存已经通过校验的字体元数据。
     *
     * 调用时机：
     * - FontBinLoader 校验 header、offset、文件大小、最大字形缓存约束后调用。
     *
     * 参数作用：
     * - glyphCount/indexOffset/bitmapOffset/fileSize/lineHeight/maxGlyphBytes/indexBytes
     *   都来自已校验的 header 或实际文件大小，后续查字形和状态查询都依赖这些值。
     */
    void configure(uint32_t glyphCount,
                   uint32_t indexOffset,
                   uint32_t bitmapOffset,
                   uint32_t fileSize,
                   uint16_t lineHeight,
                   uint16_t maxGlyphBytes,
                   uint32_t indexBytes);

    /**
     * 将运行态标记为可用。
     *
     * 调用时机：
     * - FontBinLoader 完成 header 校验、元数据保存和可选 PSRAM 索引缓存后调用。
     */
    void markReady();

    /**
     * 记录失败并释放所有运行态资源。
     *
     * 调用时机：
     * - FontBinLoader 在加载、索引读取或格式校验失败时调用。
     *
     * 返回值：
     * - 始终返回 false，便于 loader 直接 `return runtime.fail(...)`。
     */
    bool fail(const char* message);

    /**
     * 更新最近错误但不释放资源。
     *
     * 调用时机：
     * - 已加载字体在 getGlyph() 阶段发生临时读取错误或发现损坏 entry 时调用。
     */
    void setLastError(const char* message);

    /**
     * 从当前字体文件的绝对 offset 读取固定长度数据。
     *
     * 调用时机：
     * - FontBinLoader 读取 header、SPIFFS index entry、单字形 bitmap 时调用。
     *
     * 返回值：
     * - true: 成功读取 size 字节。
     * - false: 文件未打开、目标缓冲为空、seek/read 失败。
     */
    bool readAt(uint32_t offset, uint8_t* dst, size_t size);

    /**
     * 移动文件读指针。
     *
     * 调用时机：
     * - FontBinLoader 连续读取索引表到 PSRAM 时调用，减少每条索引的重复 seek。
     */
    bool seek(uint32_t offset);

    /**
     * 从当前文件读指针连续读取数据。
     *
     * 调用时机：
     * - FontBinLoader 在 seek(indexOffset) 后连续读取 raw index entry。
     */
    bool readCurrent(uint8_t* dst, size_t size);

    /**
     * 按当前 indexBytes() 尝试申请 PSRAM 索引缓存。
     *
     * 调用时机：
     * - FontBinLoader 校验 header 后，根据配置尝试把索引放入 PSRAM。
     *
     * 返回值：
     * - true: 已成功申请 mutableIndexCache() 可写缓冲。
     * - false: 配置不允许、预算不足、字形数量为空或 PSRAM 不足；这不是致命错误。
     */
    bool allocateIndexCache(bool preferPsram, size_t maxIndexCacheBytes);

    bool allocateBitmapCache(bool preferPsram, size_t maxBitmapCacheBytes);
    void closeFileIfFullyCached();

    /**
     * 释放 PSRAM 索引缓存。
     *
     * 调用时机：
     * - reset()、fail()、LRU unload、end() 或重新加载前。
     */
    void releaseIndexCache();
    void releaseBitmapCache();

    /**
     * 返回可写 PSRAM 索引缓存。
     *
     * 调用时机：
     * - FontBinLoader 申请成功后，把解码后的 GlyphEntry 写入该数组。
     */
    GlyphEntry* mutableIndexCache();

    /**
     * 返回只读 PSRAM 索引缓存。
     *
     * 调用时机：
     * - FontBinLoader::getGlyph() 二分查找时优先使用。
     */
    const GlyphEntry* indexCache() const;

    /**
     * 返回单字形 bitmap 缓存。
     *
     * 调用时机：
     * - FontBinLoader::getGlyph() 将目标字形 bitmap 读入这里，并把该指针返回给 Gui。
     *
     * 生命周期：
     * - 指针只在下一次 getGlyph()、unload() 或 end() 前有效。
     */
    uint8_t* glyphCache();
    uint8_t* mutableBitmapCache();
    const uint8_t* bitmapCache() const;

    size_t glyphCacheSize() const;
    size_t bitmapCacheSize() const;
    bool ready() const;
    bool indexInPsram() const;
    bool bitmapInPsram() const;
    const char* lastError() const;
    uint32_t glyphCount() const;
    uint32_t indexOffset() const;
    uint32_t bitmapOffset() const;
    uint32_t fileSize() const;
    uint16_t lineHeight() const;
    uint16_t maxGlyphBytes() const;
    uint32_t indexBytes() const;
    uint32_t bitmapBytes() const;

private:
    /**
     * 关闭 SPIFFS 文件句柄。
     *
     * 释放时机：
     * - reset()/fail()/析构统一调用。
     */
    void closeFile();

    // 当前字体 bin 文件句柄；open() 成功后保持打开，reset()/析构关闭。
    fs::File file_;

    // 是否已经完成格式校验并可查字形；只有 FontBinLoader::load() 成功后才为 true。
    bool ready_;

    // 最近状态或错误说明；只保存静态字符串，避免嵌入式运行期堆分配。
    const char* lastError_;

    // header 中的字形数量；用于二分查找边界和 PSRAM 索引数组长度。
    uint32_t glyphCount_;

    // index 区在文件中的绝对 offset；未缓存索引时按它从 SPIFFS 读取 raw entry。
    uint32_t indexOffset_;

    // bitmap 区在文件中的绝对 offset；字形 entry 的 bitmapOffset 是相对该位置的偏移。
    uint32_t bitmapOffset_;

    // 实际文件大小；用于 header 校验和单字形 bitmap 越界检查。
    uint32_t fileSize_;

    // 字体行高；加载成功后 FontManager 会同步到稳定 Font.lineHeight。
    uint16_t lineHeight_;

    // header 声明的最大单字形 bitmap 字节数；用于防止 glyphCache_ 越界。
    uint16_t maxGlyphBytes_;

    // PSRAM 中的解码索引数组；为空表示未缓存，getGlyph() 会回退到 SPIFFS index 读取。
    GlyphEntry* indexEntries_;

    // indexEntries_ 是否位于 PSRAM；用于诊断页面显示和 FontManager 统计缓存预算。
    bool indexInPsram_;

    // 原始 index 区字节数；由 glyphCount * raw entry size 得到，用于预算和状态查询。
    uint32_t indexBytes_;

    // 整段 bitmap 区缓存。存在时 getGlyph 直接从 PSRAM 拷贝，避免 SPIFFS 随机小读。
    uint8_t* bitmapCache_;

    // bitmapCache_ 是否位于 PSRAM，用于诊断日志。
    bool bitmapInPsram_;

    // bitmap 区原始字节数，等于 fileSize - bitmapOffset。
    uint32_t bitmapBytes_;

    // 固定单字形 bitmap 缓存；避免每次绘制为小 bitmap 分配/释放堆内存。
    uint8_t glyphCache_[kGlyphCacheSize];
};
