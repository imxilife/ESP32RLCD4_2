#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>
#include <ui/gui/fonts/FontRuntime.h>

/**
 * FontBinLoader
 *
 * 类的作用：
 * - FontBinLoader 是 F24BIN1 v1 字体 bin 的“加载和解析行为”封装。
 * - 它负责打开流程编排、header 校验、索引解码、字形查找和 bitmap 读取。
 * - 它不长期保存某个字体的运行状态；文件句柄、PSRAM 索引、glyph 缓存和错误状态都保存在
 *   FontRuntime 中。
 * - 它不负责 SPIFFS 挂载、字体注册、fallback 链和 LRU 淘汰；这些由 FontManager 统一管理。
 *
 * 流程：
 * step1: FontManager 调用 load(fs, path, config, runtime)。
 * step2: load() 让 runtime 打开字体文件。
 * step3: load() 读取并校验 F24BIN1 header、版本、flags、offset 和文件大小。
 * step4: load() 把校验后的元数据写入 runtime。
 * step5: load() 按配置让 runtime 尝试申请 PSRAM 索引缓存，并把索引解码进去。
 * step6: getGlyph() 对 runtime 的索引做二分查找，找到后按需读取单个 bitmap 到 runtime 缓存。
 * step7: unload() 释放 runtime 持有的文件句柄和 PSRAM 索引。
 *
 * 设计约束：
 * - 一个 FontBinLoader 可以服务多个 FontRuntime，因为它本身不持有单个字体状态。
 * - PSRAM 索引缓存失败不是功能失败；只会退回 SPIFFS 按需读取 index entry。
 * - bitmap 单字形缓存由 FontRuntime 固定持有，避免绘制路径频繁小块动态分配。
 */
class FontBinLoader {
public:
    /**
     * 加载并校验一个字体 bin。
     *
     * 调用时机：
     * - FontManager 首次需要某个字体，或者 LRU 卸载后再次使用该字体时调用。
     *
     * 返回值：
     * - true: runtime 已持有可用字体文件、元数据和可选 PSRAM 索引，后续可调用 getGlyph()。
     * - false: 文件缺失、打开失败、格式校验失败或索引缓存读取失败；错误写入 runtime.lastError()。
     *
     * 失败行为：
     * - 会通过 runtime.fail() 关闭文件并释放已经申请的资源，避免半加载状态残留。
     */
    bool load(fs::FS& fs,
              const char* path,
              bool preferPsram,
              size_t maxIndexCacheBytes,
              size_t maxBitmapCacheBytes,
              FontRuntime& runtime) const;

    /**
     * 卸载一个字体运行态。
     *
     * 调用时机：
     * - FontManager 手动 unload、LRU 淘汰、unloadAll 或 end 时调用。
     *
     * 行为：
     * - 释放 runtime 中的 SPIFFS 文件句柄和 PSRAM 索引缓存。
     */
    void unload(FontRuntime& runtime) const;

    /**
     * 按 Unicode codepoint 查找并读取字形。
     *
     * 调用时机：
     * - Gui::drawText()/measureTextWidth() 通过 Font.getGlyph thunk 进入 FontManager 后调用。
     *
     * 返回值：
     * - 非空：指向 runtime 内部单字形 bitmap 缓存，并写出 w/h/strideBytes/advanceX。
     * - nullptr：字体未加载、码点不存在或读取失败。码点不存在不更新错误；读取失败会更新 runtime.lastError()。
     *
     * 生命周期：
     * - 返回指针只在下一次 getGlyph()、unload() 或 end() 前有效。
     */
    const uint8_t* getGlyph(FontRuntime& runtime,
                            uint32_t codepoint,
                            int& w,
                            int& h,
                            int& strideBytes,
                            int& advanceX) const;

    bool getGlyphMetrics(FontRuntime& runtime,
                         uint32_t codepoint,
                         int& w,
                         int& h,
                         int& strideBytes,
                         int& advanceX) const;

private:
    static constexpr uint16_t kExpectedVersion = 1;
    static constexpr uint16_t kHeaderSize = 64;
    static constexpr uint16_t kIndexEntrySize = 16;

    /**
     * 从 runtime 读取一条索引。
     *
     * 行为：
     * - 有 PSRAM 索引缓存时直接从缓存取。
     * - 没有缓存时从 SPIFFS raw index entry 读取并解码。
     */
    bool readIndexEntry(FontRuntime& runtime, uint32_t index, FontRuntime::GlyphEntry& entry) const;
    bool findIndexEntry(FontRuntime& runtime, uint32_t codepoint, FontRuntime::GlyphEntry& entry) const;

    /**
     * 将完整索引表读取并解码到 runtime 的 PSRAM 缓存。
     *
     * 行为：
     * - 同时校验 codepoint 严格递增，保证后续二分查找有效。
     */
    bool readIndexCache(FontRuntime& runtime, FontRuntime::GlyphEntry* dst) const;

    /**
     * 解码一条 16 字节 F24BIN1 raw index entry。
     *
     * 约束：
     * - 字段布局必须与 tools/gen_project_font_bins.py 的 INDEX_STRUCT 保持一致。
     */
    void decodeIndexEntry(const uint8_t* raw, FontRuntime::GlyphEntry& entry) const;
};
