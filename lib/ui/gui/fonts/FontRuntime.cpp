#include "FontRuntime.h"

#include <esp_heap_caps.h>

/**
 * FontRuntime.cpp
 *
 * 类的作用：
 * - 本文件只实现单个字体“加载后运行态”的资源保存和释放。
 * - F24BIN1 header 字段如何解释、索引如何解码、字形如何二分查找，都不在这里决定；
 *   这些由 FontBinLoader 调用本类提供的文件和缓存操作完成。
 *
 * 关键流程：
 * step1: open() 打开 SPIFFS 文件并记录实际文件大小。
 * step2: configure() 保存已经被 FontBinLoader 校验过的元数据。
 * step3: allocateIndexCache() 尝试在 PSRAM 申请索引缓存。
 * step4: FontBinLoader 通过 mutableIndexCache() 写入解码索引，并调用 markReady()。
 * step5: getGlyph 阶段通过 readAt()/glyphCache() 读取单个字形 bitmap。
 * step6: reset()/fail()/析构关闭文件并释放 PSRAM。
 */

FontRuntime::FontRuntime()
    : file_(),
      ready_(false),
      lastError_("not loaded"),
      glyphCount_(0),
      indexOffset_(0),
      bitmapOffset_(0),
      fileSize_(0),
      lineHeight_(0),
      maxGlyphBytes_(0),
      indexEntries_(nullptr),
      indexInPsram_(false),
      indexBytes_(0),
      bitmapCache_(nullptr),
      bitmapInPsram_(false),
      bitmapBytes_(0),
      glyphCache_{} {}

FontRuntime::~FontRuntime() {
    reset("not loaded");
}

// 释放该字体所有运行态资源，并把元数据回到空状态。
// glyphCache_ 不主动清零：ready_ 为 false 后外部拿不到旧 bitmap 指针，避免无意义的 1KB 写入。
void FontRuntime::reset(const char* error) {
    closeFile();
    releaseIndexCache();
    releaseBitmapCache();
    ready_ = false;
    lastError_ = (error != nullptr) ? error : "not loaded";
    glyphCount_ = 0;
    indexOffset_ = 0;
    bitmapOffset_ = 0;
    fileSize_ = 0;
    lineHeight_ = 0;
    maxGlyphBytes_ = 0;
    indexBytes_ = 0;
    bitmapBytes_ = 0;
}

// 打开字体文件并记录实际文件大小。格式校验留给 FontBinLoader，避免 Runtime 绑定具体 bin 版本。
bool FontRuntime::open(fs::FS& fs, const char* path) {
    reset("not loaded");

    if (path == nullptr || path[0] == '\0') {
        return fail("empty font path");
    }
    if (!fs.exists(path)) {
        return fail("font file missing");
    }

    file_ = fs.open(path, "r");
    if (!file_) {
        return fail("open font file failed");
    }
    if (file_.isDirectory()) {
        return fail("font path is directory");
    }

    fileSize_ = static_cast<uint32_t>(file_.size());
    return true;
}

// 保存 loader 已经校验通过的 header 元数据。这里不再重复做格式判断，避免职责重叠。
void FontRuntime::configure(uint32_t glyphCount,
                            uint32_t indexOffset,
                            uint32_t bitmapOffset,
                            uint32_t fileSize,
                            uint16_t lineHeight,
                            uint16_t maxGlyphBytes,
                            uint32_t indexBytes) {
    glyphCount_ = glyphCount;
    indexOffset_ = indexOffset;
    bitmapOffset_ = bitmapOffset;
    fileSize_ = fileSize;
    lineHeight_ = lineHeight;
    maxGlyphBytes_ = maxGlyphBytes;
    indexBytes_ = indexBytes;
    bitmapBytes_ = (fileSize_ > bitmapOffset_) ? (fileSize_ - bitmapOffset_) : 0;
}

// 标记运行态可用。只有到这一步，FontManager::isLoaded() 才应该看到 loaded=true。
void FontRuntime::markReady() {
    ready_ = true;
    lastError_ = "ok";
}

// 失败路径统一入口：记录错误、关闭文件、释放 PSRAM，并返回 false 方便调用方透传。
bool FontRuntime::fail(const char* message) {
    reset((message != nullptr) ? message : "unknown error");
    return false;
}

// 只更新错误说明，不改变加载状态。用于 getGlyph 阶段的局部读取错误。
void FontRuntime::setLastError(const char* message) {
    lastError_ = (message != nullptr) ? message : "unknown error";
}

// 绝对 offset 读取。所有边界判断由调用方基于已校验元数据完成。
bool FontRuntime::readAt(uint32_t offset, uint8_t* dst, size_t size) {
    if (!file_ || dst == nullptr) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    if (!file_.seek(offset, SeekSet)) {
        return false;
    }
    return file_.read(dst, size) == size;
}

// 连续读取前的 seek 包装。该方法只移动文件指针，不解释数据内容。
bool FontRuntime::seek(uint32_t offset) {
    if (!file_) {
        return false;
    }
    return file_.seek(offset, SeekSet);
}

// 从当前读指针读取。用于一次性顺序加载 index cache，减少循环中的 seek 次数。
bool FontRuntime::readCurrent(uint8_t* dst, size_t size) {
    if (!file_ || dst == nullptr) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    return file_.read(dst, size) == size;
}

// 尝试申请 PSRAM 索引缓存。申请失败不改变 lastError_，因为系统可以退回 SPIFFS 按需读索引。
bool FontRuntime::allocateIndexCache(bool preferPsram, size_t maxIndexCacheBytes) {
    releaseIndexCache();

    if (!preferPsram || glyphCount_ == 0 || indexBytes_ == 0) {
        return false;
    }
    if (indexBytes_ > maxIndexCacheBytes) {
        return false;
    }
    if (glyphCount_ > SIZE_MAX / sizeof(GlyphEntry)) {
        return false;
    }

    const size_t cacheBytes = static_cast<size_t>(glyphCount_) * sizeof(GlyphEntry);
    GlyphEntry* cache = static_cast<GlyphEntry*>(
        heap_caps_malloc(cacheBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (cache == nullptr) {
        return false;
    }

    indexEntries_ = cache;
    indexInPsram_ = true;
    return true;
}

bool FontRuntime::allocateBitmapCache(bool preferPsram, size_t maxBitmapCacheBytes) {
    releaseBitmapCache();

    if (!preferPsram || bitmapBytes_ == 0) {
        return false;
    }
    if (bitmapBytes_ > maxBitmapCacheBytes) {
        return false;
    }

    uint8_t* cache = static_cast<uint8_t*>(
        heap_caps_malloc(static_cast<size_t>(bitmapBytes_), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (cache == nullptr) {
        return false;
    }

    bitmapCache_ = cache;
    bitmapInPsram_ = true;
    return true;
}

// 索引和 bitmap 都已缓存后，后续查找不再需要 SPIFFS 文件句柄，可主动关闭降低常驻资源占用。
void FontRuntime::closeFileIfFullyCached() {
    if (indexEntries_ != nullptr && bitmapCache_ != nullptr) {
        closeFile();
    }
}

// 释放索引缓存。indexBytes_ 作为文件元数据保留，由 reset() 再清零。
void FontRuntime::releaseIndexCache() {
    if (indexEntries_ != nullptr) {
        heap_caps_free(indexEntries_);
        indexEntries_ = nullptr;
    }
    indexInPsram_ = false;
}

void FontRuntime::releaseBitmapCache() {
    if (bitmapCache_ != nullptr) {
        heap_caps_free(bitmapCache_);
        bitmapCache_ = nullptr;
    }
    bitmapInPsram_ = false;
}

FontRuntime::GlyphEntry* FontRuntime::mutableIndexCache() {
    return indexEntries_;
}

const FontRuntime::GlyphEntry* FontRuntime::indexCache() const {
    return indexEntries_;
}

uint8_t* FontRuntime::glyphCache() {
    return glyphCache_;
}

uint8_t* FontRuntime::mutableBitmapCache() {
    return bitmapCache_;
}

const uint8_t* FontRuntime::bitmapCache() const {
    return bitmapCache_;
}

size_t FontRuntime::glyphCacheSize() const {
    return kGlyphCacheSize;
}

size_t FontRuntime::bitmapCacheSize() const {
    return static_cast<size_t>(bitmapBytes_);
}

bool FontRuntime::ready() const {
    return ready_;
}

bool FontRuntime::indexInPsram() const {
    return indexInPsram_;
}

bool FontRuntime::bitmapInPsram() const {
    return bitmapInPsram_;
}

const char* FontRuntime::lastError() const {
    return lastError_;
}

uint32_t FontRuntime::glyphCount() const {
    return glyphCount_;
}

uint32_t FontRuntime::indexOffset() const {
    return indexOffset_;
}

uint32_t FontRuntime::bitmapOffset() const {
    return bitmapOffset_;
}

uint32_t FontRuntime::fileSize() const {
    return fileSize_;
}

uint16_t FontRuntime::lineHeight() const {
    return lineHeight_;
}

uint16_t FontRuntime::maxGlyphBytes() const {
    return maxGlyphBytes_;
}

uint32_t FontRuntime::indexBytes() const {
    return indexBytes_;
}

uint32_t FontRuntime::bitmapBytes() const {
    return bitmapBytes_;
}

void FontRuntime::closeFile() {
    if (file_) {
        file_.close();
    }
}
