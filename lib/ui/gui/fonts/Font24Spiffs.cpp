#include "Font24Spiffs.h"

#include <cstring>

namespace {

constexpr uint8_t kMagic[8] = {'F', '2', '4', 'B', 'I', 'N', '1', '\0'};
constexpr uint16_t kFlag1bppMsbLeft = 0x0003;

uint16_t readLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

Font24Spiffs gFont24Spiffs;

Font24Spiffs::Font24Spiffs()
    : font_{24, &Font24Spiffs::getGlyphThunk, this, nullptr},
      ready_(false),
      lastError_("not initialized"),
      glyphCount_(0),
      indexOffset_(0),
      bitmapOffset_(0),
      fileSize_(0),
      lineHeight_(24),
      maxGlyphBytes_(0),
      glyphCache_{} {}

bool Font24Spiffs::begin(fs::FS& fs, const char* path) {
    end();
    lastError_ = "not initialized";

    if (path == nullptr || path[0] == '\0') {
        return fail("empty font path");
    }

    file_ = fs.open(path, "r");
    if (!file_) {
        return fail("open font file failed");
    }

    uint8_t header[kHeaderSize];
    if (!readAt(0, header, sizeof(header))) {
        return fail("read header failed");
    }

    const uint32_t actualFileSize = static_cast<uint32_t>(file_.size());
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

    // Header validation is deliberately strict so future incompatible bin
    // layouts fail early instead of returning misaligned glyph bytes.
    if (std::memcmp(header, kMagic, sizeof(kMagic)) != 0) {
        return fail("bad font magic");
    }
    if (version != kExpectedVersion) {
        return fail("unsupported font version");
    }
    if (headerSize != kHeaderSize || indexEntrySize != kIndexEntrySize) {
        return fail("bad header/index size");
    }
    if ((flags & kFlag1bppMsbLeft) != kFlag1bppMsbLeft) {
        return fail("unsupported bitmap flags");
    }
    if (glyphCount == 0 || indexOffset != headerSize) {
        return fail("bad index metadata");
    }
    if (bitmapOffset != indexOffset + glyphCount * indexEntrySize) {
        return fail("bad bitmap offset");
    }
    if (declaredFileSize != actualFileSize || bitmapOffset >= actualFileSize) {
        return fail("bad file size metadata");
    }
    if (lineHeight == 0 || maxGlyphBytes == 0 || maxGlyphBytes > kGlyphCacheSize) {
        return fail("bad glyph cache metadata");
    }

    glyphCount_ = glyphCount;
    indexOffset_ = indexOffset;
    bitmapOffset_ = bitmapOffset;
    fileSize_ = actualFileSize;
    lineHeight_ = lineHeight;
    maxGlyphBytes_ = maxGlyphBytes;
    font_.lineHeight = lineHeight_;
    ready_ = true;
    lastError_ = "ok";
    return true;
}

void Font24Spiffs::end() {
    if (file_) {
        file_.close();
    }
    ready_ = false;
    glyphCount_ = 0;
    indexOffset_ = 0;
    bitmapOffset_ = 0;
    fileSize_ = 0;
    maxGlyphBytes_ = 0;
}

bool Font24Spiffs::ready() const {
    return ready_;
}

const Font* Font24Spiffs::font() const {
    return ready_ ? &font_ : nullptr;
}

const char* Font24Spiffs::lastError() const {
    return lastError_;
}

bool Font24Spiffs::fail(const char* message) {
    if (file_) {
        file_.close();
    }
    ready_ = false;
    lastError_ = message;
    return false;
}

bool Font24Spiffs::readAt(uint32_t offset, uint8_t* dst, size_t size) {
    if (!file_ || dst == nullptr) {
        return false;
    }
    if (!file_.seek(offset, SeekSet)) {
        return false;
    }
    return file_.read(dst, size) == size;
}

bool Font24Spiffs::readIndexEntry(uint32_t index, GlyphEntry& entry) {
    if (index >= glyphCount_) {
        return false;
    }

    uint8_t raw[kIndexEntrySize];
    const uint32_t offset = indexOffset_ + index * kIndexEntrySize;
    if (!readAt(offset, raw, sizeof(raw))) {
        return false;
    }

    entry.codepoint = readLe32(raw + 0x00);
    entry.bitmapOffset = readLe32(raw + 0x04);
    entry.width = readLe16(raw + 0x08);
    entry.height = readLe16(raw + 0x0A);
    entry.strideBytes = readLe16(raw + 0x0C);
    entry.advanceX = readLe16(raw + 0x0E);
    return true;
}

const uint8_t* Font24Spiffs::getGlyph(uint32_t codepoint,
                                      int& w, int& h, int& strideBytes,
                                      int& advanceX) {
    if (!ready_) {
        return nullptr;
    }

    // Binary search keeps RAM usage low: no full index cache is required for
    // v1, and each probe reads exactly one 16-byte index entry from SPIFFS.
    uint32_t left = 0;
    uint32_t right = glyphCount_;
    GlyphEntry entry = {};
    while (left < right) {
        const uint32_t mid = left + (right - left) / 2;
        if (!readIndexEntry(mid, entry)) {
            lastError_ = "read index failed";
            return nullptr;
        }
        if (entry.codepoint < codepoint) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    if (left >= glyphCount_ || !readIndexEntry(left, entry) || entry.codepoint != codepoint) {
        return nullptr;
    }

    const uint32_t glyphBytes =
        static_cast<uint32_t>(entry.strideBytes) * static_cast<uint32_t>(entry.height);
    if (entry.width == 0 || entry.height == 0 || entry.strideBytes == 0 ||
        entry.advanceX == 0 || glyphBytes == 0 || glyphBytes > maxGlyphBytes_ ||
        glyphBytes > kGlyphCacheSize) {
        lastError_ = "bad glyph entry";
        return nullptr;
    }

    const uint32_t bitmapSize = fileSize_ - bitmapOffset_;
    if (entry.bitmapOffset > bitmapSize || glyphBytes > bitmapSize - entry.bitmapOffset) {
        lastError_ = "glyph bitmap out of range";
        return nullptr;
    }

    if (!readAt(bitmapOffset_ + entry.bitmapOffset, glyphCache_, glyphBytes)) {
        lastError_ = "read glyph bitmap failed";
        return nullptr;
    }

    w = entry.width;
    h = entry.height;
    strideBytes = entry.strideBytes;
    advanceX = entry.advanceX;
    return glyphCache_;
}

const uint8_t* Font24Spiffs::getGlyphThunk(uint32_t codepoint,
                                           int& w, int& h, int& strideBytes,
                                           int& advanceX,
                                           const void* data) {
    Font24Spiffs* self = static_cast<Font24Spiffs*>(const_cast<void*>(data));
    if (self == nullptr) {
        return nullptr;
    }
    return self->getGlyph(codepoint, w, h, strideBytes, advanceX);
}
