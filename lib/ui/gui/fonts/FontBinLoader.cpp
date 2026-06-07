#include "FontBinLoader.h"

#include <cstring>

/**
 * FontBinLoader.cpp
 *
 * 类的作用：
 * - 本文件实现 F24BIN1 v1 字体文件的加载、校验、索引解析和字形读取。
 * - FontBinLoader 现在是公共操作器，不保存 file/index/cache 等运行态字段。
 * - 单个字体加载后的所有状态都写入 FontRuntime，因此 FontManager 可以清晰地区分：
 *   “FontSlot 是一个字体槽位”、“FontRuntime 是加载后的状态”、“FontBinLoader 是加载行为”。
 *
 * 字体文件读取流程：
 * step1: load() 接收已挂载的 fs::FS、字体路径和目标 FontRuntime。
 * step2: runtime.open() 打开 SPIFFS 字体文件并记录实际文件大小。
 * step3: load() 读取 64 字节 header，校验 magic/version/entry size/flags/offset/file size。
 * step4: load() 调用 runtime.configure() 保存 glyphCount、indexOffset、bitmapOffset、lineHeight 等元数据。
 * step5: load() 尝试把索引解码到 runtime 的 PSRAM 缓存；失败则回退 SPIFFS 按需读索引。
 * step6: getGlyph() 对 codepoint 做二分查找，找到后只读取目标字形 bitmap 到 runtime.glyphCache()。
 * step7: unload() 调用 runtime.reset()，释放文件句柄和 PSRAM 索引。
 */

namespace {

// F24BIN1 v1 文件头 magic，用于快速排除旧格式、空文件或损坏文件。
constexpr uint8_t kMagic[8] = {'F', '2', '4', 'B', 'I', 'N', '1', '\0'};

// 当前运行时只支持 1bpp、MSB-left、逐行存储的 bitmap。
constexpr uint16_t kFlag1bppMsbLeft = 0x0003;

// 从 F24BIN1 小端字段读取 16 位整数。显式解析便于跨平台审查格式。
uint16_t readLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

// 从 F24BIN1 小端字段读取 32 位整数，用于 offset、文件大小和 Unicode codepoint。
uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

// 加载单个字体文件。runtime 是状态承载者，loader 只负责按 F24BIN1 规则把状态填进去。
bool FontBinLoader::load(fs::FS& fs,
                         const char* path,
                         bool preferPsram,
                         size_t maxIndexCacheBytes,
                         size_t maxBitmapCacheBytes,
                         FontRuntime& runtime) const {
    if (!runtime.open(fs, path)) {
        return false;
    }
    if (runtime.fileSize() < kHeaderSize) {
        return runtime.fail("font file too small");
    }

    // header 固定 64 字节，字段布局必须与生成脚本保持一致。
    uint8_t header[kHeaderSize];
    if (!runtime.readAt(0, header, sizeof(header))) {
        return runtime.fail("read header failed");
    }

    const uint32_t actualFileSize = runtime.fileSize();
    const uint16_t version = readLe16(header + 0x08);
    const uint16_t headerSize = readLe16(header + 0x0A);
    const uint16_t indexEntrySize = readLe16(header + 0x0C);
    const uint16_t flags = readLe16(header + 0x0E);
    const uint32_t glyphCount = readLe32(header + 0x10);
    const uint32_t indexOffset = readLe32(header + 0x14);
    const uint32_t bitmapOffset = readLe32(header + 0x18);
    const uint32_t declaredFileSize = readLe32(header + 0x1C);
    const uint16_t lineHeight = readLe16(header + 0x20);
    const uint16_t maxGlyphBytes = readLe16(header + 0x24);

    // Header 校验保持严格：格式不匹配时尽早失败，避免后续绘制错位或越界读取。
    if (std::memcmp(header, kMagic, sizeof(kMagic)) != 0) {
        return runtime.fail("bad font magic");
    }
    if (version != kExpectedVersion) {
        return runtime.fail("unsupported font version");
    }
    if (headerSize != kHeaderSize || indexEntrySize != kIndexEntrySize) {
        return runtime.fail("bad header/index size");
    }
    if ((flags & kFlag1bppMsbLeft) != kFlag1bppMsbLeft) {
        return runtime.fail("unsupported bitmap flags");
    }
    if (glyphCount == 0 || indexOffset != headerSize) {
        return runtime.fail("bad index metadata");
    }
    if (bitmapOffset != indexOffset + glyphCount * indexEntrySize) {
        return runtime.fail("bad bitmap offset");
    }
    if (declaredFileSize != actualFileSize || bitmapOffset >= actualFileSize) {
        return runtime.fail("bad file size metadata");
    }
    if (lineHeight == 0 || maxGlyphBytes == 0 ||
        maxGlyphBytes > FontRuntime::kGlyphCacheSize) {
        return runtime.fail("bad glyph cache metadata");
    }

    const uint32_t indexBytes = glyphCount * kIndexEntrySize;
    runtime.configure(glyphCount,
                      indexOffset,
                      bitmapOffset,
                      actualFileSize,
                      lineHeight,
                      maxGlyphBytes,
                      indexBytes);

    // PSRAM 索引缓存是性能优化，不是功能前提。分配失败时保留文件句柄，后续按需从 SPIFFS 读索引。
    if (runtime.allocateIndexCache(preferPsram, maxIndexCacheBytes)) {
        if (!readIndexCache(runtime, runtime.mutableIndexCache())) {
            return runtime.fail("read index cache failed");
        }
    }

    // Bitmap cache is an optional startup-time tradeoff: one sequential SPIFFS read
    // avoids many tiny random reads while drawing text.
    if (runtime.allocateBitmapCache(preferPsram, maxBitmapCacheBytes)) {
        if (!runtime.seek(runtime.bitmapOffset()) ||
            !runtime.readCurrent(runtime.mutableBitmapCache(), runtime.bitmapCacheSize())) {
            return runtime.fail("read bitmap cache failed");
        }
    }

    runtime.closeFileIfFullyCached();
    runtime.markReady();
    return true;
}

// 卸载运行态。FontManager 仍保留稳定 Font 描述符，后续再次访问可以重新 load。
void FontBinLoader::unload(FontRuntime& runtime) const {
    runtime.reset("not loaded");
}

// 查找并读取单个字形。fallback 由 Gui 沿 Font.fallback 继续调用，不在 loader 内处理。
const uint8_t* FontBinLoader::getGlyph(FontRuntime& runtime,
                                       uint32_t codepoint,
                                       int& w,
                                       int& h,
                                       int& strideBytes,
                                       int& advanceX) const {
    if (!runtime.ready()) {
        return nullptr;
    }

    // 索引按 Unicode codepoint 升序排列，二分查找兼顾速度和内存占用。
    uint32_t left = 0;
    uint32_t right = runtime.glyphCount();
    FontRuntime::GlyphEntry entry = {};
    while (left < right) {
        const uint32_t mid = left + (right - left) / 2;
        if (!readIndexEntry(runtime, mid, entry)) {
            runtime.setLastError("read index failed");
            return nullptr;
        }
        if (entry.codepoint < codepoint) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    if (left >= runtime.glyphCount() ||
        !readIndexEntry(runtime, left, entry) ||
        entry.codepoint != codepoint) {
        return nullptr;
    }

    // 单字形 bitmap 字节数必须同时满足 header 最大值和本地固定缓存上限。
    const uint32_t glyphBytes =
        static_cast<uint32_t>(entry.strideBytes) * static_cast<uint32_t>(entry.height);
    if (entry.width == 0 || entry.height == 0 || entry.strideBytes == 0 ||
        entry.advanceX == 0 || glyphBytes == 0 || glyphBytes > runtime.maxGlyphBytes() ||
        glyphBytes > runtime.glyphCacheSize()) {
        runtime.setLastError("bad glyph entry");
        return nullptr;
    }

    // entry.bitmapOffset 是相对 bitmap 区的偏移，不是文件绝对 offset。
    const uint32_t bitmapSize = runtime.fileSize() - runtime.bitmapOffset();
    if (entry.bitmapOffset > bitmapSize || glyphBytes > bitmapSize - entry.bitmapOffset) {
        runtime.setLastError("glyph bitmap out of range");
        return nullptr;
    }

    uint8_t* cache = runtime.glyphCache();
    const uint8_t* bitmapCache = runtime.bitmapCache();
    if (bitmapCache != nullptr) {
        std::memcpy(cache, bitmapCache + entry.bitmapOffset, glyphBytes);
    } else {
        if (!runtime.readAt(runtime.bitmapOffset() + entry.bitmapOffset, cache, glyphBytes)) {
            runtime.setLastError("read glyph bitmap failed");
            return nullptr;
        }
    }

    w = entry.width;
    h = entry.height;
    strideBytes = entry.strideBytes;
    advanceX = entry.advanceX;
    return cache;
}

bool FontBinLoader::getGlyphMetrics(FontRuntime& runtime,
                                    uint32_t codepoint,
                                    int& w,
                                    int& h,
                                    int& strideBytes,
                                    int& advanceX) const {
    FontRuntime::GlyphEntry entry = {};
    if (!findIndexEntry(runtime, codepoint, entry)) {
        return false;
    }

    const uint32_t glyphBytes =
        static_cast<uint32_t>(entry.strideBytes) * static_cast<uint32_t>(entry.height);
    if (entry.width == 0 || entry.height == 0 || entry.strideBytes == 0 ||
        entry.advanceX == 0 || glyphBytes == 0 || glyphBytes > runtime.maxGlyphBytes() ||
        glyphBytes > runtime.glyphCacheSize()) {
        runtime.setLastError("bad glyph entry");
        return false;
    }

    const uint32_t bitmapSize = runtime.fileSize() - runtime.bitmapOffset();
    if (entry.bitmapOffset > bitmapSize || glyphBytes > bitmapSize - entry.bitmapOffset) {
        runtime.setLastError("glyph bitmap out of range");
        return false;
    }

    w = entry.width;
    h = entry.height;
    strideBytes = entry.strideBytes;
    advanceX = entry.advanceX;
    return true;
}

bool FontBinLoader::findIndexEntry(FontRuntime& runtime,
                                   uint32_t codepoint,
                                   FontRuntime::GlyphEntry& entry) const {
    if (!runtime.ready()) {
        return false;
    }

    uint32_t left = 0;
    uint32_t right = runtime.glyphCount();
    while (left < right) {
        const uint32_t mid = left + (right - left) / 2;
        if (!readIndexEntry(runtime, mid, entry)) {
            runtime.setLastError("read index failed");
            return false;
        }
        if (entry.codepoint < codepoint) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    if (left >= runtime.glyphCount()) {
        return false;
    }
    if (!readIndexEntry(runtime, left, entry)) {
        runtime.setLastError("read index failed");
        return false;
    }
    return entry.codepoint == codepoint;
}

// 读取单条索引。优先使用 PSRAM 解码缓存；没有缓存时从 SPIFFS raw index 读取。
bool FontBinLoader::readIndexEntry(FontRuntime& runtime,
                                   uint32_t index,
                                   FontRuntime::GlyphEntry& entry) const {
    if (index >= runtime.glyphCount()) {
        return false;
    }

    const FontRuntime::GlyphEntry* cache = runtime.indexCache();
    if (cache != nullptr) {
        entry = cache[index];
        return true;
    }

    uint8_t raw[kIndexEntrySize];
    const uint32_t offset = runtime.indexOffset() + index * kIndexEntrySize;
    if (!runtime.readAt(offset, raw, sizeof(raw))) {
        return false;
    }
    decodeIndexEntry(raw, entry);
    return true;
}

// 顺序读取并解码完整索引表到 PSRAM。这里验证 codepoint 递增，防止二分查找前提被破坏。
bool FontBinLoader::readIndexCache(FontRuntime& runtime, FontRuntime::GlyphEntry* dst) const {
    if (dst == nullptr || !runtime.seek(runtime.indexOffset())) {
        return false;
    }

    uint8_t raw[kIndexEntrySize];
    uint32_t prevCodepoint = 0;
    for (uint32_t i = 0; i < runtime.glyphCount(); ++i) {
        if (!runtime.readCurrent(raw, sizeof(raw))) {
            return false;
        }
        decodeIndexEntry(raw, dst[i]);
        if (i > 0 && dst[i].codepoint <= prevCodepoint) {
            return false;
        }
        prevCodepoint = dst[i].codepoint;
    }
    return true;
}

// 解码 raw index entry。字段顺序来自生成脚本，不在 Runtime 中实现，避免 Runtime 绑定格式细节。
void FontBinLoader::decodeIndexEntry(const uint8_t* raw, FontRuntime::GlyphEntry& entry) const {
    entry.codepoint = readLe32(raw + 0x00);
    entry.bitmapOffset = readLe32(raw + 0x04);
    entry.width = readLe16(raw + 0x08);
    entry.height = readLe16(raw + 0x0A);
    entry.strideBytes = readLe16(raw + 0x0C);
    entry.advanceX = readLe16(raw + 0x0E);
}
