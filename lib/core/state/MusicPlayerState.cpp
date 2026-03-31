#include "MusicPlayerState.h"

#include <core/state_manager/StateManager.h>
#include <ui/assets/Mp3ControlIcons.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_20_20.h>
#include <ui/gui/fonts/Font_ascii_IBMPlexSans_Medium_24_Var.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>

namespace {
constexpr int kScreenW = 400;
constexpr int kScreenH = 300;
constexpr int kPageMargin = 12;
constexpr int kGap = 10;
constexpr int kListX = 12;
constexpr int kListY = 18;
constexpr int kListW = 150;
constexpr int kListH = 270;
constexpr int kRightX = kListX + kListW + kGap;
constexpr int kRightY = 18;
constexpr int kRightW = kScreenW - kPageMargin - kRightX;
constexpr int kRightH = 270;
constexpr int kRowH = 38;
constexpr int kRadius = 10;
constexpr int kPlaylistLabelX = 50;
constexpr int kPlaylistLabelW = kListW - 62;
constexpr uint32_t kMarqueeStepMs = 120;
constexpr uint32_t kMarqueeHoldSteps = 5;

const Font* kListFont = &kFont_ascii_IBMPlexSans_Medium_20_20;
const Font* kAsciiTitleFont = &kFont_ascii_IBMPlexSans_Medium_24_Var;
const Font* kChineseTitleFont = &kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20;
const Font* kInfoFont = &kFont_ascii_IBMPlexSans_Medium_20_20;

String marqueeTextToWidth(Gui& gui, const String& text, const Font* font, int maxWidth, uint32_t nowMs) {
    if (text.isEmpty() || font == nullptr) return text;
    if (gui.measureTextWidth(text.c_str(), font) <= maxWidth) return text;

    const String spacer = "   ";
    const String loop = text + spacer + text;
    const int textChars = text.length();
    const uint32_t step = nowMs / kMarqueeStepMs;
    const uint32_t cycle = static_cast<uint32_t>(textChars) + kMarqueeHoldSteps * 2U;
    const uint32_t phase = cycle == 0 ? 0 : step % cycle;

    int startChar = 0;
    if (phase < kMarqueeHoldSteps) {
        startChar = 0;
    } else if (phase < kMarqueeHoldSteps + static_cast<uint32_t>(textChars)) {
        startChar = static_cast<int>(phase - kMarqueeHoldSteps);
    } else {
        startChar = textChars;
    }

    String visible = loop.substring(startChar);
    while (!visible.isEmpty() && gui.measureTextWidth(visible.c_str(), font) > maxWidth) {
        visible.remove(visible.length() - 1);
    }
    return visible;
}

String fitTextToWidth(Gui& gui, const String& text, const Font* font, int maxWidth) {
    if (text.isEmpty() || font == nullptr) return text;
    if (gui.measureTextWidth(text.c_str(), font) <= maxWidth) return text;

    String result = text;
    while (result.length() > 1) {
        result.remove(result.length() - 1);
        String candidate = result + "...";
        if (gui.measureTextWidth(candidate.c_str(), font) <= maxWidth) return candidate;
    }
    return "...";
}

const char* stateLabel(Mp3Controller::State state) {
    switch (state) {
    case Mp3Controller::State::UNINITIALIZED: return "INIT";
    case Mp3Controller::State::IDLE: return "READY";
    case Mp3Controller::State::PLAYING: return "PLAYING";
    case Mp3Controller::State::PAUSED: return "PAUSED";
    case Mp3Controller::State::STOPPED: return "STOPPED";
    case Mp3Controller::State::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}
}

MusicPlayerState::MusicPlayerState(Gui& gui)
    : gui_(gui),
      controller_(Mp3Controller::instance()) {}

void MusicPlayerState::onEnter() {
    controller_.beginIfNeeded();
    const Mp3Controller::Snapshot snap = controller_.snapshot();
    selectedIndex_ = snap.currentIndex >= 0 ? snap.currentIndex : snap.selectedIndex;
    ensureSelectionVisible();
    lastRenderedIndex_ = -2;
    lastRenderedState_ = Mp3Controller::State::UNINITIALIZED;
    lastRenderedProgressBucket_ = UINT32_MAX;
    lastRenderedMarqueeBucket_ = UINT32_MAX;
    drawUI();
}

void MusicPlayerState::onExit() {}

void MusicPlayerState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void MusicPlayerState::onKeyEvent(const KeyEvent& event) {
    if (event.id == KeyId::KEY1 && event.action == KeyAction::LONG_PRESS) {
        Serial.println("[Music] KEY1 LONG_PRESS -> back to launch");
        requestTransition(StateId::LAUNCH);
        return;
    }

    if (event.action != KeyAction::DOWN && event.action != KeyAction::LONG_PRESS) return;

    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        controller_.selectNext();
        selectedIndex_ = controller_.selectedIndex();
        ensureSelectionVisible();
        drawUI();
        return;
    }

    if (event.id == KeyId::KEY2 && event.action == KeyAction::LONG_PRESS) {
        controller_.next();
        const Mp3Controller::Snapshot snap = controller_.snapshot();
        syncSelectionToCurrent(snap);
        drawUI();
        return;
    }

    if (event.id == KeyId::KEY2 && event.action == KeyAction::DOWN) {
        const Mp3Controller::Snapshot snap = controller_.snapshot();
        if (!snap.hasPlaylist) {
            drawUI();
            return;
        }

        controller_.setSelectedIndex(selectedIndex_);
        if (snap.currentIndex == selectedIndex_ &&
            (snap.state == Mp3Controller::State::PLAYING || snap.state == Mp3Controller::State::PAUSED)) {
            controller_.togglePause();
        } else {
            controller_.play(selectedIndex_);
        }
        syncSelectionToCurrent(controller_.snapshot());
        drawUI();
    }
}

void MusicPlayerState::tick() {
    controller_.tick();
    const Mp3Controller::Snapshot snap = controller_.snapshot();
    if (snap.currentIndex != lastRenderedIndex_ && snap.currentIndex >= 0) {
        syncSelectionToCurrent(snap);
    }

    const uint32_t progressBucket = snap.progressMs / 1000U;
    const uint32_t marqueeBucket = millis() / kMarqueeStepMs;
    if (lastRenderedIndex_ != snap.currentIndex ||
        lastRenderedState_ != snap.state ||
        lastRenderedProgressBucket_ != progressBucket ||
        (hasVisibleOverflowLabels(snap) && lastRenderedMarqueeBucket_ != marqueeBucket)) {
        drawUI();
    }
}

void MusicPlayerState::ensureSelectionVisible() {
    if (selectedIndex_ < listTopIndex_) {
        listTopIndex_ = selectedIndex_;
    }
    if (selectedIndex_ >= listTopIndex_ + kVisibleRows) {
        listTopIndex_ = selectedIndex_ - kVisibleRows + 1;
    }
    if (listTopIndex_ < 0) listTopIndex_ = 0;
}

void MusicPlayerState::syncSelectionToCurrent(const Mp3Controller::Snapshot& snap) {
    if (snap.currentIndex >= 0) {
        selectedIndex_ = snap.currentIndex;
        controller_.setSelectedIndex(selectedIndex_);
        ensureSelectionVisible();
    }
}

void MusicPlayerState::drawUI() {
    const Mp3Controller::Snapshot snap = controller_.snapshot();
    gui_.clear();
    gui_.fillRoundRect(kListX, kListY, kListW, kListH, kRadius, ColorWhite);
    gui_.drawRoundRect(kListX, kListY, kListW, kListH, kRadius, ColorBlack);
    gui_.fillRoundRect(kRightX, kRightY, kRightW, kRightH, kRadius, ColorWhite);
    gui_.drawRoundRect(kRightX, kRightY, kRightW, kRightH, kRadius, ColorBlack);

    drawPlaylist(snap);
    drawNowPlaying(snap);

    lastRenderedIndex_ = snap.currentIndex;
    lastRenderedState_ = snap.state;
    lastRenderedProgressBucket_ = snap.progressMs / 1000U;
    lastRenderedMarqueeBucket_ = millis() / kMarqueeStepMs;
}

bool MusicPlayerState::hasVisibleOverflowLabels(const Mp3Controller::Snapshot& snap) const {
    if (!snap.hasPlaylist) return false;
    if (selectedIndex_ < listTopIndex_ || selectedIndex_ >= listTopIndex_ + kVisibleRows) return false;
    const String& title = controller_.trackAt(selectedIndex_).title;
    return gui_.measureTextWidth(title.c_str(), kListFont) > kPlaylistLabelW;
}

void MusicPlayerState::drawPlaylist(const Mp3Controller::Snapshot& snap) {
    gui_.setFont(kListFont);
    gui_.drawText(kListX + 12, kListY + 10, "PLAYLIST", ColorBlack, ColorWhite);
    gui_.drawLine(kListX + 12, kListY + 34, kListX + kListW - 12, kListY + 34, ColorBlack);

    if (!snap.hasPlaylist) {
        gui_.drawText(kListX + 12, kListY + 54, "NO MP3", ColorBlack, ColorWhite);
        return;
    }

    for (int row = 0; row < kVisibleRows; ++row) {
        const int index = listTopIndex_ + row;
        if (index >= snap.playlistCount) break;

        const int rowY = kListY + 42 + row * kRowH;
        const bool selected = index == selectedIndex_;
        const bool playing = index == snap.currentIndex;
        if (selected) {
            gui_.fillRoundRect(kListX + 8, rowY, kListW - 16, kRowH - 6, 8, ColorBlack);
        }

        char prefix[8];
        snprintf(prefix, sizeof(prefix), "%02d", index + 1);
        gui_.drawText(kListX + 16, rowY + 8, prefix,
                      selected ? ColorWhite : ColorBlack,
                      selected ? ColorBlack : ColorWhite);

        const uint8_t rowBg = selected ? ColorBlack : ColorWhite;
        gui_.fillRect(kListX + 46, rowY, kPlaylistLabelW, kRowH - 6, rowBg);

        const String& title = controller_.trackAt(index).title;
        const String visibleTitle = selected
            ? marqueeTextToWidth(gui_, title, kListFont, kPlaylistLabelW, millis())
            : fitTextToWidth(gui_, title, kListFont, kPlaylistLabelW);
        gui_.drawText(kListX + kPlaylistLabelX, rowY + 8, visibleTitle.c_str(),
                      selected ? ColorWhite : ColorBlack,
                      selected ? ColorBlack : ColorWhite);
        gui_.drawText(kListX + 16, rowY + 8, prefix,
                      selected ? ColorWhite : ColorBlack,
                      selected ? ColorBlack : ColorWhite);

        if (playing) {
            gui_.fillCircle(kListX + kListW - 18, rowY + 14, 4, selected ? ColorWhite : ColorBlack);
        }
    }
}

void MusicPlayerState::drawNowPlaying(const Mp3Controller::Snapshot& snap) {
    const int innerX = kRightX + 16;
    const int innerW = kRightW - 32;

    String title = "NO MUSIC";
    const Font* titleFont = kAsciiTitleFont;
    if (snap.hasPlaylist && !snap.track.title.isEmpty()) {
        title = fitTextToWidth(gui_, snap.track.title, titleFont, innerW);
    } else if (!snap.errorText.isEmpty()) {
        title = fitTextToWidth(gui_, snap.errorText, kAsciiTitleFont, innerW);
    }

    gui_.setFont(titleFont);
    gui_.drawText(innerX, kRightY + 18, title.c_str(), ColorBlack, ColorWhite);

    char line1[48];
    if (snap.hasPlaylist && snap.playlistCount > 0) {
        const int displayIndex = (snap.currentIndex >= 0) ? (snap.currentIndex + 1) : (selectedIndex_ + 1);
        snprintf(line1, sizeof(line1), "%s  %d/%d", stateLabel(snap.state), displayIndex, snap.playlistCount);
    } else {
        snprintf(line1, sizeof(line1), "%s", stateLabel(snap.state));
    }
    gui_.setFont(kInfoFont);
    gui_.drawText(innerX, kRightY + 54, line1, ColorBlack, ColorWhite);

    char line2[48];
    if (snap.track.sizeBytes > 0) {
        snprintf(line2, sizeof(line2), "%lu KB",
                 static_cast<unsigned long>(snap.track.sizeBytes / 1024UL));
    } else {
        snprintf(line2, sizeof(line2), "0 KB");
    }
    gui_.drawText(innerX, kRightY + 80, line2, ColorBlack, ColorWhite);

    const int barX = innerX;
    const int barY = kRightY + 122;
    const int barW = innerW;
    gui_.drawLine(barX, barY, barX + barW, barY, ColorBlack);

    int knobX = barX + barW / 2;
    if (snap.progressKnown && snap.track.durationMs > 0) {
        knobX = barX + static_cast<int>((static_cast<uint64_t>(barW) * snap.progressMs) / snap.track.durationMs);
        if (knobX < barX) knobX = barX;
        if (knobX > barX + barW) knobX = barX + barW;
    }
    gui_.fillCircle(knobX, barY, 4, ColorBlack);

    const int iconY = kRightY + 146;
    const int leftIconX = kRightX + 34;
    const int centerIconX = kRightX + kRightW / 2 - Mp3ControlIcons::kIconW / 2;
    const int rightIconX = kRightX + kRightW - 34 - Mp3ControlIcons::kIconW;
    gui_.drawBitmap(leftIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    Mp3ControlIcons::kBackward, ColorBlack);
    gui_.drawBitmap(centerIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    snap.state == Mp3Controller::State::PLAYING ? Mp3ControlIcons::kPause
                                                                : Mp3ControlIcons::kPlay,
                    ColorBlack);
    gui_.drawBitmap(rightIconX, iconY, Mp3ControlIcons::kIconW, Mp3ControlIcons::kIconH,
                    Mp3ControlIcons::kForward, ColorBlack);

}
