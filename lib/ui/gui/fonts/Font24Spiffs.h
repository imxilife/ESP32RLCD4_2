#pragma once

#include <FS.h>
#include <stdint.h>
#include <ui/gui/Font.h>

/**
 * Font24Spiffs
 *
 * Runtime Font adapter for /font24.bin stored in SPIFFS.
 * The bin keeps a sorted Unicode index, so Gui can continue to pass UTF-8
 * strings while this adapter resolves decoded codepoints to 1bpp bitmaps.
 */
class Font24Spiffs {
public:
    Font24Spiffs();

    /// Mount-independent initialization: caller owns SPIFFS.begin().
    bool begin(fs::FS& fs, const char* path = "/font24.bin");

    /// Close the open font file and mark the adapter unavailable.
    void end();

    bool ready() const;
    const Font* font() const;
    const char* lastError() const;

private:
    struct GlyphEntry {
        uint32_t codepoint;
        uint32_t bitmapOffset;
        uint16_t width;
        uint16_t height;
        uint16_t strideBytes;
        uint16_t advanceX;
    };

    static constexpr uint16_t kExpectedVersion = 1;
    static constexpr uint16_t kHeaderSize = 64;
    static constexpr uint16_t kIndexEntrySize = 16;
    static constexpr size_t kGlyphCacheSize = 72;

    fs::File file_;
    Font font_;
    bool ready_;
    const char* lastError_;

    uint32_t glyphCount_;
    uint32_t indexOffset_;
    uint32_t bitmapOffset_;
    uint32_t fileSize_;
    uint16_t lineHeight_;
    uint16_t maxGlyphBytes_;
    uint8_t glyphCache_[kGlyphCacheSize];

    bool fail(const char* message);
    bool readAt(uint32_t offset, uint8_t* dst, size_t size);
    bool readIndexEntry(uint32_t index, GlyphEntry& entry);
    const uint8_t* getGlyph(uint32_t codepoint, int& w, int& h, int& strideBytes, int& advanceX);

    static const uint8_t* getGlyphThunk(uint32_t codepoint,
                                        int& w, int& h, int& strideBytes,
                                        int& advanceX,
                                        const void* data);
};

extern Font24Spiffs gFont24Spiffs;
