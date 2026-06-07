#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>
#include <ui/gui/Font.h>
#include <ui/gui/fonts/FontBinLoader.h>
#include <ui/gui/fonts/FontRuntime.h>

enum class FontId : uint8_t {
    ZhMain,
    ZhSub,
    EnMain,
    EnSub,
    Digits60,
};

/**
 * FontManagerConfig
 *
 * 类的作用：
 * - 定义字体管理器的运行时缓存策略。
 *
 * 属性的作用：
 * - maxLoadedFonts: LRU 同时保留的已加载字体数量，超过后卸载最久未用字体。
 * - maxIndexCacheBytes: 允许保留的 PSRAM 索引缓存总预算，超过后配合 LRU 释放旧字体。
 * - maxBitmapCacheBytes: 单个字体 bitmap 缓存上限；不是所有字体 bitmap 的总预算。
 * - preferPsram: 是否优先把索引表放入 PSRAM；关闭时仍可从 SPIFFS 按需读取索引。
 */
struct FontManagerConfig {
    uint8_t maxLoadedFonts = 5;
    size_t maxIndexCacheBytes = 160 * 1024;
    size_t maxBitmapCacheBytes = 384 * 1024;
    bool preferPsram = true;
};

/**
 * FontStatus
 *
 * 类的作用：
 * - 给诊断页面和串口日志暴露单个字体的当前运行状态。
 *
 * 属性的作用：
 * - loaded: FontRuntime 是否已经打开字体文件并通过格式校验。
 * - indexInPsram: 索引表是否已经缓存到 PSRAM。
 * - path: 该字体在 SPIFFS 中的固定路径。
 * - error: 最近一次加载或读取错误；未加载时通常为 "not loaded"。
 * - glyphCount: 字体文件中的字形数量；未加载时为 0。
 * - indexBytes: 原始索引表字节数；未加载时为 0。
 */
struct FontStatus {
    bool loaded;
    bool indexInPsram;
    const char* path;
    const char* error;
    uint32_t glyphCount;
    uint32_t indexBytes;
};

/**
 * FontManager
 *
 * 类的作用：
 * - FontManager 是项目字体系统的统一入口。
 * - 它负责 SPIFFS 挂载、字体规格注册、稳定 Font 描述符、fallback 链、懒加载、LRU 卸载和状态查询。
 * - 它向 GUI 层提供稳定 const Font*，让 Gui::setFont/drawText/measureTextWidth 不关心 SPIFFS、
 *   PSRAM、FontRuntime 或 F24BIN1 解析细节。
 * - 它不直接解析字体 bin；具体格式校验、索引解析和字形读取由 FontBinLoader 完成。
 *
 * 字体加载到使用的流程：
 * step1: main.cpp 在 setup() 中调用 FontManager::instance().init()。
 * step2: init() 挂载 SPIFFS，并注册项目字体规格。
 * step3: setupSlots() 为每个字体建立稳定 Font 描述符和 fallback 链，但不打开 bin 文件。
 * step4: 页面通过 font(FontId) 获取稳定 const Font*，并交给 Gui::setFont()。
 * step5: Gui::drawText() 或 Gui::measureTextWidth() 解码 UTF-8，得到 Unicode codepoint。
 * step6: Gui 调用 Font.getGlyph，也就是 FontManager::getGlyphThunk()。
 * step7: getGlyphThunk() 根据 Font.data 找回 FontSlot，并调用 FontManager::getGlyph()。
 * step8: getGlyph() 调用 load(FontId)；如果目标字体未加载，公共 loader_ 会把字体加载到该槽位的 FontRuntime。
 * step9: FontBinLoader 打开 SPIFFS bin、校验 F24BIN1 header，并把运行态元数据写入 FontRuntime。
 * step10: FontRuntime 优先持有 PSRAM 索引缓存；如果缓存失败，后续按需从 SPIFFS 读取索引。
 * step11: FontBinLoader 对 codepoint 做二分查找，并将单个字形 bitmap 读入 FontRuntime 的小缓存。
 * step12: Gui 拿到 bitmap / width / height / stride / advanceX 后写入帧缓冲。
 * step13: 如果当前字体没有该码点，Gui 沿 Font.fallback 链继续查找，fallback 字体同样会懒加载。
 * step14: 每次字体访问都会 touch() 更新 LRU，trimLoadedFonts() 在超限时卸载最久未用字体的 FontRuntime。
 * step15: end() 或 LRU unload() 释放 FontRuntime 中的文件句柄和 PSRAM 索引，稳定 Font 描述符保留在槽位中。
 *
 * 资源约束：
 * - Font 描述符存放在 manager 内部，生命周期覆盖 init() 到 end()。
 * - FontRuntime 持有单个字体加载后的文件句柄、PSRAM 索引和单字形缓存，LRU 或 end() 释放。
 * - FontBinLoader 是公共加载/解析工具，不保存每个字体的私有运行态。
 * - 不保留旧字体命名空间兼容层，调用方必须使用 FontManager::instance().font(FontId)。
 */
class FontManager {
public:
    /**
     * 获取全局单例。
     *
     * 返回值：
     * - 始终返回同一个 manager 实例；字体系统资源由 init()/end() 控制。
     */
    static FontManager& instance();

    /**
     * 初始化字体系统。
     *
     * 调用时机：
     * - main.cpp 的 setup() 中显示初始化后调用一次。
     *
     * 返回值：
     * - true: SPIFFS 挂载成功，字体规格表和稳定 Font 描述符已建立。
     * - false: SPIFFS 挂载失败，页面拿不到可用字体。
     *
     * 注意：
     * - init() 不打开字体 bin，单个字体错误会在首次 load()/getGlyph() 时暴露。
     */
    bool init(const FontManagerConfig& config = FontManagerConfig{});

    /**
     * 释放字体系统。
     *
     * 调用时机：
     * - 系统关闭、重新挂载 SPIFFS 或测试重置时调用。
     */
    void end();

    /**
     * 获取稳定 Font 指针。
     *
     * 返回值：
     * - init() 成功且 FontId 有效时返回稳定 Font*。
     * - 未初始化或 FontId 无效时返回 nullptr。
     *
     * 注意：
     * - 此方法不会加载字体文件，真正加载发生在 getGlyph thunk 或显式 load()。
     */
    const Font* font(FontId id);

    /**
     * 显式加载某个字体。
     *
     * 调用时机：
     * - 通常由 getGlyph thunk 懒加载触发；诊断页也可以主动调用。
     *
     * 返回值：
     * - true: 字体文件已打开并可查字形。
     * - false: 未初始化、文件缺失或格式校验失败；错误见 lastError()/status()。
     */
    bool load(FontId id);

    /**
     * 卸载单个字体。
     *
     * 调用时机：
     * - LRU 淘汰、诊断操作或手动释放时调用。
     */
    void unload(FontId id);

    /**
     * 卸载所有字体。
     *
     * 调用时机：
     * - end() 内部或需要清空全部字体缓存时调用。
     */
    void unloadAll();

    /**
     * 查询指定字体是否已加载。
     *
     * 返回值：
     * - true: 对应 FontRuntime 已通过 FontBinLoader 校验并可读取字形。
     */
    bool isLoaded(FontId id) const;

    /**
     * 获取指定字体诊断状态。
     *
     * 返回值：
     * - 有效 FontId 返回该字体路径、加载状态、索引缓存和错误信息。
     * - 无效 FontId 返回 invalid font id 状态。
     */
    FontStatus status(FontId id) const;

    const char* lastError() const;
    const char* failedPath() const;

private:
    static constexpr uint8_t kFontCount = 5;
    static constexpr uint8_t kNoFallback = 0xFF;

    struct FontSpec {
        FontId id;
        const char* name;
        const char* path;
        uint16_t defaultLineHeight;
        uint8_t fallbackIndex;
    };

    /**
     * FontSlot
     *
     * 类的作用：
     * - 表示“一个项目字体槽位”，也就是一个 FontId 对应的固定规格、稳定 Font 描述符和运行态。
     * - 一个字体需要一个 FontSlot，因为每个字体都有独立 path、fallback、Font 描述符、FontRuntime 和 LRU 时间。
     * - FontSlot 不负责解析 bin；公共 loader_ 会操作它内部的 runtime。
     *
     * 属性的作用：
     * - spec: 指向静态字体规格表，init() 绑定，生命周期不需要释放。
     * - font: 暴露给 Gui 的稳定 Font 描述符；runtime 卸载后仍保留，便于再次懒加载。
     * - runtime: 该字体加载后的运行态，持有文件句柄、PSRAM 索引、glyph 缓存和最近错误。
     * - lastUse: LRU 访问序号，0 表示未加载或已卸载，值越小越久未使用。
     */
    struct FontSlot {
        FontSlot()
            : spec(nullptr),
              font{0, nullptr, nullptr, nullptr, nullptr},
              runtime(),
              lastUse(0),
              loadFailed(false) {}

        const FontSpec* spec;
        Font font;
        FontRuntime runtime;
        uint32_t lastUse;

        // 单次启动内的失败熔断标记。文件缺失或格式错误时，继续重试只会反复卡 SPIFFS；
        // 重新刷入 SPIFFS 后设备会重启，init()/setupSlots() 会清掉该标记。
        bool loadFailed;
    };

    static const FontSpec kSpecs[kFontCount];

    FontManager();
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    static const uint8_t* getGlyphThunk(uint32_t codepoint,
                                        int& w,
                                        int& h,
                                        int& strideBytes,
                                        int& advanceX,
                                        const void* data);
    static bool getGlyphMetricsThunk(uint32_t codepoint,
                                     int& w,
                                     int& h,
                                     int& strideBytes,
                                     int& advanceX,
                                     const void* data);

    static bool isValidId(FontId id);
    static uint8_t toIndex(FontId id);

    FontSlot* slotFor(FontId id);
    const FontSlot* slotFor(FontId id) const;
    const uint8_t* getGlyph(FontSlot& slot,
                            uint32_t codepoint,
                            int& w,
                            int& h,
                            int& strideBytes,
                            int& advanceX);
    bool getGlyphMetrics(FontSlot& slot,
                         uint32_t codepoint,
                         int& w,
                         int& h,
                         int& strideBytes,
                         int& advanceX);
    void setupSlots();
    void touch(FontSlot& slot);
    void trimLoadedFonts(FontId keepId);
    uint8_t loadedCount() const;
    size_t loadedIndexBytes() const;
    void setFailure(const char* path, const char* error);

    // SPIFFS 挂载成功后保存文件系统指针；end() 会清空并调用 SPIFFS.end()。
    fs::FS* fs_;

    // init() 是否成功。font()/load()/getGlyph() 都依赖它判断字体系统是否可用。
    bool initialized_;

    // 当前缓存策略，init(config) 写入；LRU 和 loader 加载索引时读取。
    FontManagerConfig config_;

    // 公共字体加载器。它不持有单个字体状态，只操作各 FontSlot 中的 FontRuntime。
    FontBinLoader loader_;

    // 字体槽位。Font 指针稳定，runtime 可随 LRU 加载/卸载。
    FontSlot slots_[kFontCount];

    // 单调递增访问序号，用于 LRU 判断；只在字体访问时递增。
    uint32_t accessClock_;

    // 全局最近错误，便于 main.cpp 和诊断页快速打印。
    const char* lastError_;
    const char* failedPath_;
};
