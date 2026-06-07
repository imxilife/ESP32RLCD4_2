#include "FontTestState.h"

#include <Arduino.h>
#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/FontManager.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr int kScreenW = 400;
constexpr int kZhMainY = 50;
constexpr int kZhSubY = 90;
constexpr int kEnMainY = 145;
constexpr int kEnSubY = 205;
constexpr int kLineGap = 4;
constexpr size_t kLineBufferSize = 96;
constexpr size_t kWordBufferSize = 32;
constexpr bool kEnableGlyphProbe = false;

constexpr const char* kChineseSample =
    "\xe7\x99\xbd\xe6\x97\xa5\xe4\xbe\x9d\xe5\xb1\xb1\xe5\xb0\xbd"
    "\xef\xbc\x8c\xe9\xbb\x84\xe6\xb2\xb3\xe5\x85\xa5\xe6\xb5\xb7\xe6\xb5\x81";
constexpr const char* kEnglishSample = "To be or not to be, that is the question";

struct TextProbe {
    int width = 0;
    int glyphCount = 0;
    int missingCount = 0;
    uint32_t firstMissing = 0;
};

bool decodeNextUtf8(const char*& p, uint32_t& codepoint) {
    if (p == nullptr || *p == '\0') return false;

    const uint8_t* s = reinterpret_cast<const uint8_t*>(p);
    const uint8_t c = *s;
    if (c < 0x80) {
        codepoint = c;
        ++p;
        return true;
    }

    int extraBytes = 0;
    if ((c & 0xE0) == 0xC0) {
        extraBytes = 1;
        codepoint = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extraBytes = 2;
        codepoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extraBytes = 3;
        codepoint = c & 0x07;
    } else {
        ++p;
        return false;
    }

    ++s;
    for (int i = 0; i < extraBytes; ++i) {
        const uint8_t cc = *s;
        if ((cc & 0xC0) != 0x80) {
            ++p;
            return false;
        }
        codepoint = (codepoint << 6) | (cc & 0x3F);
        ++s;
    }
    p = reinterpret_cast<const char*>(s);
    return true;
}

TextProbe probeTextGlyphs(const char* text, const Font* font) {
    TextProbe probe;
    if (text == nullptr || font == nullptr) return probe;

    const char* p = text;
    uint32_t codepoint = 0;
    while (decodeNextUtf8(p, codepoint)) {
        int w = 0;
        int h = 0;
        int stride = 0;
        int advanceX = 0;
        const uint8_t* glyph = nullptr;
        for (const Font* f = font; f != nullptr; f = f->fallback) {
            if (f->getGlyph == nullptr) continue;
            glyph = f->getGlyph(codepoint, w, h, stride, advanceX, f->data);
            if (glyph != nullptr && w > 0 && h > 0 && stride > 0) break;
        }

        if (glyph != nullptr && advanceX > 0) {
            ++probe.glyphCount;
            probe.width += advanceX;
        } else {
            ++probe.missingCount;
            if (probe.firstMissing == 0) probe.firstMissing = codepoint;
            probe.width += 16;
        }
    }
    return probe;
}

const Font* fontFor(FontId id) {
    return FontManager::instance().font(id);
}

void logFontStatus(const char* tag, FontId id) {
    const FontStatus status = FontManager::instance().status(id);
    Serial.printf("[FontTest] status=%s loaded=%d psram=%d glyphs=%lu indexBytes=%lu path=%s error=%s\n",
                  tag ? tag : "",
                  status.loaded ? 1 : 0,
                  status.indexInPsram ? 1 : 0,
                  static_cast<unsigned long>(status.glyphCount),
                  static_cast<unsigned long>(status.indexBytes),
                  status.path ? status.path : "",
                  status.error ? status.error : "");
}

void logAllFontStatus(const char* phase) {
    Serial.printf("[FontTest] font status phase=%s failed=%s error=%s\n",
                  phase ? phase : "",
                  FontManager::instance().failedPath() ? FontManager::instance().failedPath() : "",
                  FontManager::instance().lastError());
    logFontStatus("zhMain", FontId::ZhMain);
    logFontStatus("zhSub", FontId::ZhSub);
    logFontStatus("enMain", FontId::EnMain);
    logFontStatus("enSub", FontId::EnSub);
    logFontStatus("digits60", FontId::Digits60);
}
}  // namespace

FontTestState::FontTestState(Gui& gui)
    : gui_(gui) {}

void FontTestState::onEnter() {
    const uint32_t enterStartMs = millis();
    Serial.printf("[FontTest] onEnter start t=%lu\n",
                  static_cast<unsigned long>(enterStartMs));
    logAllFontStatus("enter");
    drawScreen();
    logAllFontStatus("after-draw");
    Serial.printf("[FontTest] onEnter end dur=%lu\n",
                  static_cast<unsigned long>(millis() - enterStartMs));
}

void FontTestState::onExit() {}

void FontTestState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void FontTestState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        requestTransition(StateId::LAUNCH);
    }
}

void FontTestState::drawScreen() {
    const uint32_t screenStartMs = millis();
    gui_.clear(ColorWhite);

    Serial.printf("[FontTest] draw screen start t=%lu\n",
                  static_cast<unsigned long>(screenStartMs));
    drawTextLine("zhMain", kZhMainY, kChineseSample, fontFor(FontId::ZhMain));
    drawTextLine("zhSub", kZhSubY, kChineseSample, fontFor(FontId::ZhSub));
    drawTextLine("enMain", kEnMainY, kEnglishSample, fontFor(FontId::EnMain));
    drawTextLine("enSub", kEnSubY, kEnglishSample, fontFor(FontId::EnSub));
    Serial.printf("[FontTest] draw screen end dur=%lu\n",
                  static_cast<unsigned long>(millis() - screenStartMs));
}

void FontTestState::drawTextLine(const char* tag, int y, const char* text, const Font* font) {
    const uint32_t lineStartMs = millis();
    if (text == nullptr || font == nullptr) {
        Serial.printf("[FontTest] skip %s text=%p font=%p\n",
                      tag ? tag : "",
                      static_cast<const void*>(text),
                      static_cast<const void*>(font));
        return;
    }

    TextProbe probe;
    uint32_t probeMs = 0;
    if (kEnableGlyphProbe) {
        const uint32_t probeStartMs = millis();
        probe = probeTextGlyphs(text, font);
        probeMs = millis() - probeStartMs;
    }

    const uint32_t measureStartMs = millis();
    const int measuredW = gui_.measureTextWidth(text, font);
    const uint32_t measureMs = millis() - measureStartMs;
    Serial.printf("[FontTest] line=%s y=%d font=%p lineHeight=%d probeOn=%d probeW=%d measureW=%d glyphs=%d missing=%d firstMissing=U+%04lX probeMs=%lu measureMs=%lu\n",
                  tag ? tag : "",
                  y,
                  static_cast<const void*>(font),
                  font->lineHeight,
                  kEnableGlyphProbe ? 1 : 0,
                  probe.width,
                  measuredW,
                  probe.glyphCount,
                  probe.missingCount,
                  static_cast<unsigned long>(probe.firstMissing),
                  static_cast<unsigned long>(probeMs),
                  static_cast<unsigned long>(measureMs));

    if (measuredW <= kScreenW) {
        drawCenteredLine(tag, y, text, font, measuredW);
        Serial.printf("[FontTest] line=%s done totalMs=%lu\n",
                      tag ? tag : "",
                      static_cast<unsigned long>(millis() - lineStartMs));
        return;
    }

    // The English 18px sample is slightly wider than the 400px panel.
    // Wrap on spaces so the requested text remains complete and unclipped.
    char line[kLineBufferSize] = {};
    const char* p = text;
    int lineY = y;

    while (*p != '\0') {
        while (*p == ' ') ++p;
        if (*p == '\0') break;

        char word[kWordBufferSize] = {};
        size_t wordLen = 0;
        while (*p != '\0' && *p != ' ' && wordLen < kWordBufferSize - 1) {
            word[wordLen++] = *p++;
        }
        word[wordLen] = '\0';
        while (*p != '\0' && *p != ' ') ++p;

        char candidate[kLineBufferSize] = {};
        if (line[0] == '\0') {
            std::snprintf(candidate, sizeof(candidate), "%s", word);
        } else {
            std::snprintf(candidate, sizeof(candidate), "%s %s", line, word);
        }

        if (line[0] != '\0' && gui_.measureTextWidth(candidate, font) > kScreenW) {
            drawCenteredLine(tag, lineY, line, font, gui_.measureTextWidth(line, font));
            lineY += font->lineHeight + kLineGap;
            std::snprintf(line, sizeof(line), "%s", word);
        } else {
            std::snprintf(line, sizeof(line), "%s", candidate);
        }
    }

    if (line[0] != '\0') {
        drawCenteredLine(tag, lineY, line, font, gui_.measureTextWidth(line, font));
    }
    Serial.printf("[FontTest] line=%s done totalMs=%lu\n",
                  tag ? tag : "",
                  static_cast<unsigned long>(millis() - lineStartMs));
}

void FontTestState::drawCenteredLine(const char* tag, int y, const char* text, const Font* font, int textW) {
    if (text == nullptr || font == nullptr) return;

    int x = (kScreenW - textW) / 2;
    if (x < 0) x = 0;
    const uint32_t drawStartMs = millis();
    gui_.setFont(font);
    gui_.drawText(x, y, text, ColorBlack, ColorWhite);
    Serial.printf("[FontTest] draw=%s x=%d y=%d w=%d textBytes=%u drawMs=%lu\n",
                  tag ? tag : "",
                  x,
                  y,
                  textW,
                  static_cast<unsigned int>(std::strlen(text)),
                  static_cast<unsigned long>(millis() - drawStartMs));
}
