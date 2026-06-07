#include "LaunchState.h"

#include <Arduino.h>
#include <core/app_tasks/app_tasks.h>
#include <features/memo/MemoService.h>
#include <features/pomodoro/Pomodoro.h>
#include <features/voice_assistant/VoiceAssistantService.h>
#include <ui/assets/HomeBitmapAssets.h>
#include <ui/gui/fonts/FontManager.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr int kScreenW = 400;
constexpr int kScreenH = 300;

constexpr int kDividerX = 200;
constexpr int kDividerTop = 40;
constexpr int kDividerBottom = 253;

constexpr int kHourY = 64;
constexpr int kTimeRowGap = 12;
constexpr int kTimeDigitGap = 4;
// 首页时间滚动动画总时长。旧数字上滚和新数字入场各占一半，调这个值即可整体变快或变慢。
constexpr uint32_t kTimeRollAnimationMs = 300;
constexpr int kDateGap = 22;
constexpr int kLeftTextRight = kDividerX - 10;

constexpr int kStatusY = 8;
constexpr int kBatteryX = kScreenW - 10 - HomeBitmapAssets::kStatusIconW;
constexpr int kWifiX = kBatteryX - HomeBitmapAssets::kStatusIconW - 6;

constexpr int kRightIconX = 222;
constexpr int kRightTextX = 258;
constexpr int kRightTextMaxW = kScreenW - kRightTextX - 24;
constexpr int kWeatherY = 68;
constexpr int kPomodoroY = 126;
constexpr int kMemoY = 184;
constexpr int kPomodoroIconOffsetX = 2 - ((HomeBitmapAssets::kPomodoroIconW - HomeBitmapAssets::kHomeRowIconW) / 2);
constexpr int kPomodoroIconOffsetY = 7 - ((HomeBitmapAssets::kPomodoroIconH - HomeBitmapAssets::kHomeRowIconH) / 2);

constexpr int kVoiceX = 20;
constexpr int kVoiceY = 258;

const Font* timeFont() {
    // 首页左栏只显示两行数字，使用独立 60px 数字 bin，冒号不参与渲染。
    FontManager& fonts = FontManager::instance();
    const Font* digits60 = fonts.font(FontId::Digits60);
    if (digits60 != nullptr && fonts.load(FontId::Digits60)) {
        return digits60;
    }
    return fonts.font(FontId::EnMain);
}

const Font* temperatureFont() {
    // 天气温度使用中文主字体；数字和 U+2103 摄氏度符号都来自 font_zh_main_24.bin。
    return FontManager::instance().font(FontId::ZhMain);
}

const Font* valueFont() {
    // 番茄时间与天气温度统一使用中文主字体，保持右栏主字体层级一致。
    return FontManager::instance().font(FontId::ZhMain);
}

const Font* rightLabelFont() {
    // 右栏说明文字使用 20px 中文副字体，兼容中文天气文案和短状态标签。
    return FontManager::instance().font(FontId::ZhSub);
}

const Font* smallLabelFont() {
    return FontManager::instance().font(FontId::EnSub);
}

const Font* dateLabelFont() {
    return FontManager::instance().font(FontId::ZhSub);
}

const Font* memoFont() {
    return FontManager::instance().font(FontId::ZhSub);
}

int leftColumnCenteredX(int textW) {
    int x = (kDividerX - textW) / 2;
    if (x < 8) x = 8;
    if (x + textW > kLeftTextRight) {
        x = kLeftTextRight - textW;
        if (x < 8) x = 8;
    }
    return x;
}

int measureTimePairWidth(Gui& gui, const char* text, const Font* font) {
    if (text == nullptr || font == nullptr || text[0] == '\0' || text[1] == '\0') return 0;

    char digit[2] = {text[0], '\0'};
    const int firstW = gui.measureTextWidth(digit, font);
    digit[0] = text[1];
    const int secondW = gui.measureTextWidth(digit, font);
    return firstW + kTimeDigitGap + secondW;
}

void copyTimeDigits(char dst[5], const char* src) {
    if (dst == nullptr) return;
    if (src == nullptr) {
        std::memcpy(dst, "0000", 5);
        return;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        dst[i] = (src[i] >= '0' && src[i] <= '9') ? src[i] : '0';
    }
    dst[4] = '\0';
}

void formatTimeDigits(const RTCTime& t, char out[5]) {
    if (out == nullptr) return;
    snprintf(out, 5, "%02u%02u",
             static_cast<unsigned int>(t.hour),
             static_cast<unsigned int>(t.minute));
}

int digitWidth(Gui& gui, char digit, const Font* font) {
    if (font == nullptr || digit < '0' || digit > '9') return 0;
    char text[2] = {digit, '\0'};
    return gui.measureTextWidth(text, font);
}

int maxInt(int a, int b) {
    return a > b ? a : b;
}

const uint8_t* findDigitGlyph(const Font* font, char digit,
                              int& w, int& h, int& strideBytes, int& advanceX) {
    if (font == nullptr || digit < '0' || digit > '9') return nullptr;
    const uint32_t codepoint = static_cast<uint8_t>(digit);
    for (const Font* f = font; f != nullptr; f = f->fallback) {
        if (f->getGlyph == nullptr) continue;
        const uint8_t* glyph = f->getGlyph(codepoint, w, h, strideBytes, advanceX, f->data);
        if (glyph != nullptr && w > 0 && h > 0 && strideBytes > 0) {
            return glyph;
        }
    }
    return nullptr;
}

void drawDigitGlyphClipped(Gui& gui, int x, int y, char digit, const Font* font,
                           int clipX, int clipY, int clipW, int clipH) {
    if (clipW <= 0 || clipH <= 0) return;

    int w = 0;
    int h = 0;
    int stride = 0;
    int advanceX = 0;
    const uint8_t* glyph = findDigitGlyph(font, digit, w, h, stride, advanceX);
    if (glyph == nullptr) return;

    // 只在当前数字槽位内落点，避免滚动过程覆盖日期或相邻数字。
    const int clipRight = clipX + clipW;
    const int clipBottom = clipY + clipH;
    for (int row = 0; row < h; ++row) {
        const int py = y + row;
        if (py < clipY || py >= clipBottom) continue;
        for (int col = 0; col < w; ++col) {
            const int px = x + col;
            if (px < clipX || px >= clipRight) continue;
            const int byteIndex = row * stride + (col >> 3);
            const int bitIndex = 7 - (col & 0x7);
            if ((glyph[byteIndex] & (1U << bitIndex)) != 0) {
                gui.drawPixel(px, py, ColorBlack);
            }
        }
    }
}

void drawRolledDigit(Gui& gui, int slotX, int slotY, int slotW, int slotH,
                     char fromDigit, char toDigit, const Font* font,
                     uint32_t elapsedMs, uint32_t durationMs) {
    if (fromDigit == toDigit || durationMs == 0) {
        const int w = digitWidth(gui, toDigit, font);
        const int x = slotX + (slotW - w) / 2;
        drawDigitGlyphClipped(gui, x, slotY, toDigit, font, slotX, slotY, slotW, slotH);
        return;
    }

    uint32_t outMs = durationMs / 2;
    if (outMs == 0) outMs = 1;
    uint32_t inMs = durationMs - outMs;
    if (inMs == 0) inMs = 1;

    const int fromW = digitWidth(gui, fromDigit, font);
    const int toW = digitWidth(gui, toDigit, font);
    const int fromX = slotX + (slotW - fromW) / 2;
    const int toX = slotX + (slotW - toW) / 2;

    if (elapsedMs < outMs) {
        const int offset = -static_cast<int>((static_cast<uint64_t>(slotH) * elapsedMs) / outMs);
        drawDigitGlyphClipped(gui, fromX, slotY + offset, fromDigit, font,
                              slotX, slotY, slotW, slotH);
        return;
    }

    uint32_t inElapsed = elapsedMs - outMs;
    if (inElapsed > inMs) inElapsed = inMs;
    const int offset = slotH - static_cast<int>((static_cast<uint64_t>(slotH) * inElapsed) / inMs);
    drawDigitGlyphClipped(gui, toX, slotY + offset, toDigit, font,
                          slotX, slotY, slotW, slotH);
}

void drawAnimatedTimePair(Gui& gui, int y, const char* fromPair, const char* toPair,
                          const Font* font, uint32_t elapsedMs, uint32_t durationMs) {
    if (fromPair == nullptr || toPair == nullptr || font == nullptr) return;
    if (fromPair[0] == '\0' || fromPair[1] == '\0' ||
        toPair[0] == '\0' || toPair[1] == '\0') {
        return;
    }

    const int slotH = font->lineHeight > 0 ? font->lineHeight : 60;
    int slotW[2] = {};
    for (uint8_t i = 0; i < 2; ++i) {
        slotW[i] = maxInt(digitWidth(gui, fromPair[i], font), digitWidth(gui, toPair[i], font));
        if (slotW[i] <= 0) slotW[i] = slotH / 2;
    }

    const int rowW = slotW[0] + kTimeDigitGap + slotW[1];
    int x = leftColumnCenteredX(rowW);
    for (uint8_t i = 0; i < 2; ++i) {
        drawRolledDigit(gui, x, y, slotW[i], slotH,
                        fromPair[i], toPair[i], font, elapsedMs, durationMs);
        x += slotW[i] + kTimeDigitGap;
    }
}

void drawTimePair(Gui& gui, int x, int y, const char* text, const Font* font) {
    if (text == nullptr || font == nullptr || text[0] == '\0' || text[1] == '\0') return;

    // 首页时间只绘制两位数字；逐字绘制用于给同一组时/分数字增加固定呼吸间距。
    char digit[2] = {text[0], '\0'};
    gui.setFont(font);
    gui.drawText(x, y, digit, ColorBlack, ColorTransparent);

    x += gui.measureTextWidth(digit, font) + kTimeDigitGap;
    digit[0] = text[1];
    gui.drawText(x, y, digit, ColorBlack, ColorTransparent);
}

constexpr const char* kWeekLabelsZh[7] = {
    "周日", "周一", "周二", "周三", "周四", "周五", "周六"
};

String fitTextToWidth(Gui& gui, const char* text, const Font* font, int maxWidth) {
    if (text == nullptr || font == nullptr || maxWidth <= 0) return "";
    if (gui.measureTextWidth(text, font) <= maxWidth) return String(text);

    String result(text);
    while (result.length() > 0) {
        result.remove(result.length() - 1);
        String candidate = result + "...";
        if (gui.measureTextWidth(candidate.c_str(), font) <= maxWidth) return candidate;
    }
    return "";
}
}

LaunchState::LaunchState(Gui& gui)
    : gui_(gui) {}

void LaunchState::onEnter() {
    static bool rtcStarted = false;
    if (!rtcStarted) {
        rtcStarted = true;
        xTaskCreate(rtcTask, "rtcTask", 2048, &rtc_, 2, nullptr);
        Serial.println("[Launch] RTC task started");
    }

    startBatteryTaskOnce();
    voicePhase_ = VoiceAssistantService::phase();
    key2LongPressConsumed_ = false;
    draw();
}

void LaunchState::onExit() {}

void LaunchState::onMessage(const AppMessage& msg) {
    switch (msg.type) {
    case MSG_RTC_UPDATE:
        applyTimeUpdate(msg.rtcTime);
        draw();
        break;

    case MSG_NTP_SYNC:
        handleNtpSync(msg);
        draw();
        break;

    case MSG_WIFI_STATUS:
        wifiConnected_ = msg.wifi.connected;
        draw();
        break;

    case MSG_BATTERY_UPDATE:
        batteryVoltage_ = msg.battery.voltage;
        draw();
        break;

    case MSG_WEATHER_UPDATE:
        weather_ = msg.weather;
        Serial.printf("[Launch] weather update valid=%u temp=%d icon=%u text=%s\n",
                      static_cast<unsigned int>(weather_.valid),
                      static_cast<int>(weather_.temperatureC),
                      static_cast<unsigned int>(weather_.iconCode),
                      weather_.text);
        draw();
        break;

    case MSG_MEMO_UPDATE:
    case MSG_POMODORO_UPDATE:
        draw();
        break;

    case MSG_VOICE_ASSISTANT_UPDATE:
        voicePhase_ = msg.voiceAssistant.phase;
        draw();
        break;

    default:
        break;
    }
}

void LaunchState::onKeyEvent(const KeyEvent& event) {
    // KEY2 long press is reserved as a diagnostics shortcut to the font test page.
    if (event.id != KeyId::KEY2) return;

    if (event.action == KeyAction::LONG_PRESS) {
        key2LongPressConsumed_ = true;
        Serial.println("[Launch] Enter FontTestState from KEY2 long press");
        requestTransition(StateId::FONT_TEST);
        return;
    }

    if (event.action == KeyAction::UP) {
        if (key2LongPressConsumed_) {
            key2LongPressConsumed_ = false;
            return;
        }
    }
}

void LaunchState::tick() {
    if (!timeRoll_.active) return;

    const uint32_t elapsed = millis() - timeRoll_.startMs;
    if (elapsed >= timeRoll_.durationMs) {
        timeRoll_.active = false;
        copyTimeDigits(visibleTimeDigits_, timeRoll_.toDigits);
    }
    draw();
}

void LaunchState::draw() {
    drawShell();
    drawStatusIcons();
    drawTimeBlock();
    drawRightColumn();
    drawVoiceAssistant();
}

void LaunchState::drawShell() {
    // The physical shell provides the outer border; software renders only the white page.
    gui_.fillRect(0, 0, kScreenW, kScreenH, ColorWhite);
    gui_.drawLine(kDividerX, kDividerTop, kDividerX, kDividerBottom, ColorBlack);
}

void LaunchState::drawStatusIcons() {
    gui_.drawBitmap(kWifiX, kStatusY, HomeBitmapAssets::kStatusIconW, HomeBitmapAssets::kStatusIconH,
                    wifiConnected_ ? HomeBitmapAssets::kWifi : HomeBitmapAssets::kNoWifi,
                    ColorBlack);
    gui_.drawBitmap(kBatteryX, kStatusY, HomeBitmapAssets::kStatusIconW, HomeBitmapAssets::kStatusIconH,
                    batteryIcon(), ColorBlack);
}

void LaunchState::drawTimeBlock() {
    const Font* time = timeFont();
    int minuteY = kHourY;
    if (time != nullptr) {
        gui_.setFont(time);
        // 小时和分钟分两行绘制，间距由 kTimeRowGap 单独控制，避免数字块上下贴死。
        minuteY = kHourY + time->lineHeight + kTimeRowGap;
        if (timeRoll_.active) {
            uint32_t elapsed = millis() - timeRoll_.startMs;
            if (elapsed >= timeRoll_.durationMs) {
                elapsed = timeRoll_.durationMs;
                timeRoll_.active = false;
                copyTimeDigits(visibleTimeDigits_, timeRoll_.toDigits);
            }

            if (timeRoll_.active) {
                char fromHour[3] = {timeRoll_.fromDigits[0], timeRoll_.fromDigits[1], '\0'};
                char toHour[3] = {timeRoll_.toDigits[0], timeRoll_.toDigits[1], '\0'};
                char fromMinute[3] = {timeRoll_.fromDigits[2], timeRoll_.fromDigits[3], '\0'};
                char toMinute[3] = {timeRoll_.toDigits[2], timeRoll_.toDigits[3], '\0'};
                drawAnimatedTimePair(gui_, kHourY, fromHour, toHour, time, elapsed, timeRoll_.durationMs);
                drawAnimatedTimePair(gui_, minuteY, fromMinute, toMinute, time, elapsed, timeRoll_.durationMs);
            }
        }

        if (!timeRoll_.active) {
            char hourBuf[3] = {visibleTimeDigits_[0], visibleTimeDigits_[1], '\0'};
            int textW = measureTimePairWidth(gui_, hourBuf, time);
            drawTimePair(gui_, leftColumnCenteredX(textW), kHourY, hourBuf, time);
            char minuteBuf[3] = {visibleTimeDigits_[2], visibleTimeDigits_[3], '\0'};
            textW = measureTimePairWidth(gui_, minuteBuf, time);
            drawTimePair(gui_, leftColumnCenteredX(textW), minuteY, minuteBuf, time);
        }
    }

    char dateBuf[32];
    formatDate(dateBuf, sizeof(dateBuf));
    const Font* label = dateLabelFont();
    if (label == nullptr) return;
    gui_.setFont(label);
    const int dateW = gui_.measureTextWidth(dateBuf, label);
    int dateX = (kDividerX - dateW) / 2;
    if (dateX < 8) dateX = 8;
    if (dateX + dateW > kLeftTextRight) {
        dateX = kLeftTextRight - dateW;
        if (dateX < 8) dateX = 8;
    }
    const int dateY = minuteY + (time != nullptr ? time->lineHeight : label->lineHeight) + kDateGap;
    gui_.drawText(dateX, dateY, dateBuf, ColorBlack, ColorWhite);
}

void LaunchState::drawRightColumn() {
    drawWeatherRow(kWeatherY);
    drawPomodoroRow(kPomodoroY);
    drawMemoRow(kMemoY);
}

void LaunchState::drawWeatherRow(int y) {
    if (weather_.valid) {
        drawWeatherIcon(kRightIconX, y + 6, weather_.iconCode);
    } else {
        drawHomeRowIcon(kRightIconX, y + 6, HomeBitmapAssets::kWeatherPlaceholder);
    }
    drawTemperature(kRightTextX, y, weather_.temperatureC, weather_.valid);
    const char* weatherText = (weather_.valid && weather_.text[0] != '\0') ? weather_.text : "多云";
    drawFittedText(kRightTextX, y + 27, kRightTextMaxW,
                   weatherText, rightLabelFont());
}

void LaunchState::drawPomodoroRow(int y) {
    // 番茄钟使用 24x24 专用取模，减少 20x20 缩放造成的细节粘连。
    gui_.drawBitmap(kRightIconX + kPomodoroIconOffsetX, y + kPomodoroIconOffsetY,
                    HomeBitmapAssets::kPomodoroIconW, HomeBitmapAssets::kPomodoroIconH,
                    HomeBitmapAssets::kPomodoroClock, ColorBlack);

    const Pomodoro::Snapshot snap = PomodoroService::instance().snapshot();
    const uint32_t rem = snap.remSec;
    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu",
             static_cast<unsigned long>(rem / 60),
             static_cast<unsigned long>(rem % 60));

    const Font* value = valueFont();
    if (value == nullptr) return;
    gui_.setFont(value);
    gui_.drawText(kRightTextX, y, timeBuf, ColorBlack, ColorWhite);
    drawFittedText(kRightTextX, y + 27, kRightTextMaxW,
                   snap.finished ? "完成" : "专注", rightLabelFont());
}

void LaunchState::drawMemoRow(int y) {
    drawHomeRowIcon(kRightIconX + 3, y + 7, HomeBitmapAssets::kTodo);

    MemoItem memos[2];
    const uint8_t count = hasRtc_ ? MemoService::topToday(currentTime_, memos, 2) : 0;
    if (count == 0) {
        drawChineseWrappedText(kRightTextX, y + 1, kRightTextMaxW, "今日无事，很开心");
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        drawFittedText(kRightTextX, y + 1 + i * 22, kRightTextMaxW, memos[i].text, memoFont());
    }
}

void LaunchState::drawVoiceAssistant() {
    drawMicIcon(kVoiceX, kVoiceY - 3);
    const Font* label = smallLabelFont();
    if (label == nullptr) return;
    gui_.setFont(label);
    gui_.drawText(kVoiceX + 24, kVoiceY + 4, "Listening,", ColorBlack, ColorWhite);
}

void LaunchState::drawWeatherIcon(int x, int y, uint16_t iconCode) {
    const uint8_t* bitmap = HomeBitmapAssets::weatherIcon(iconCode);
    if (bitmap == nullptr) return;
    gui_.drawBitmap(x, y, HomeBitmapAssets::kWeatherIconW, HomeBitmapAssets::kWeatherIconH,
                    bitmap, ColorBlack);
}

void LaunchState::drawHomeRowIcon(int x, int y, const uint8_t* bitmap) {
    if (bitmap == nullptr) return;
    gui_.drawBitmap(x, y, HomeBitmapAssets::kHomeRowIconW, HomeBitmapAssets::kHomeRowIconH,
                    bitmap, ColorBlack);
}

void LaunchState::drawMicIcon(int x, int y) {
    gui_.drawRoundRect(x + 6, y, 10, 17, 5, ColorBlack);
    gui_.fillRect(x + 10, y + 17, 2, 7, ColorBlack);
    gui_.drawLine(x + 4, y + 11, x + 4, y + 15, ColorBlack);
    gui_.drawLine(x + 18, y + 11, x + 18, y + 15, ColorBlack);
    gui_.drawLine(x + 4, y + 15, x + 8, y + 20, ColorBlack);
    gui_.drawLine(x + 18, y + 15, x + 14, y + 20, ColorBlack);
    gui_.fillRect(x + 6, y + 24, 12, 2, ColorBlack);
}

void LaunchState::drawTemperature(int x, int y, int16_t temperatureC, bool valid) {
    char tempBuf[16];
    if (valid) {
        snprintf(tempBuf, sizeof(tempBuf), "%d℃", static_cast<int>(temperatureC));
    } else {
        snprintf(tempBuf, sizeof(tempBuf), "--℃");
    }

    const Font* value = temperatureFont();
    if (value == nullptr) return;
    gui_.setFont(value);
    gui_.drawText(x, y, tempBuf, ColorBlack, ColorWhite);
}

void LaunchState::drawFittedText(int x, int y, int maxWidth, const char* text, const Font* font) {
    if (text == nullptr || font == nullptr) return;
    const String fitted = fitTextToWidth(gui_, text, font, maxWidth);
    gui_.setFont(font);
    gui_.drawText(x, y, fitted.c_str(), ColorBlack, ColorWhite);
}

void LaunchState::drawChineseWrappedText(int x, int y, int maxWidth, const char* text) {
    if (text == nullptr) return;

    String line;
    const char* p = text;
    int lineY = y;
    while (*p != '\0' && lineY <= y + 24) {
        const char* charStart = p;
        uint8_t lead = static_cast<uint8_t>(*p);
        size_t len = 1;
        if ((lead & 0xE0) == 0xC0) len = 2;
        else if ((lead & 0xF0) == 0xE0) len = 3;
        else if ((lead & 0xF8) == 0xF0) len = 4;

        String next = line + String(charStart).substring(0, len);
        if (gui_.measureTextWidth(next.c_str(), memoFont()) > maxWidth && line.length() > 0) {
            gui_.setFont(memoFont());
            gui_.drawText(x, lineY, line.c_str(), ColorBlack, ColorWhite);
            line = "";
            lineY += 22;
            continue;
        }

        line = next;
        p += len;
    }

    if (line.length() > 0) {
        gui_.setFont(memoFont());
        gui_.drawText(x, lineY, line.c_str(), ColorBlack, ColorWhite);
    }
}

const uint8_t* LaunchState::batteryIcon() const {
    if (batteryVoltage_ <= 0.0f) return HomeBitmapAssets::kBatteryFull;
    if (batteryVoltage_ < 3.55f) return HomeBitmapAssets::kBattery25;
    if (batteryVoltage_ < 3.75f) return HomeBitmapAssets::kBattery50;
    if (batteryVoltage_ < 3.95f) return HomeBitmapAssets::kBattery75;
    return HomeBitmapAssets::kBatteryFull;
}

void LaunchState::formatDate(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    if (!hasRtc_) {
        snprintf(out, outSize, "周-- --月--日");
        return;
    }

    const uint8_t weekday = currentTime_.weekday < 7 ? currentTime_.weekday : 0;
    snprintf(out, outSize, "%s %u月%u日",
             kWeekLabelsZh[weekday], currentTime_.month, currentTime_.day);
}

void LaunchState::applyTimeUpdate(const RTCTime& nextTime) {
    char nextDigits[5] = "0000";
    formatTimeDigits(nextTime, nextDigits);

    if (!hasRtc_) {
        currentTime_ = nextTime;
        hasRtc_ = true;
        timeRoll_.active = false;
        copyTimeDigits(visibleTimeDigits_, nextDigits);
        return;
    }

    char fromDigits[5] = "0000";
    copyTimeDigits(fromDigits, timeRoll_.active ? timeRoll_.toDigits : visibleTimeDigits_);

    currentTime_ = nextTime;
    hasRtc_ = true;

    if (std::strncmp(fromDigits, nextDigits, 4) == 0) {
        return;
    }
    startTimeRollAnimation(fromDigits, nextDigits);
}

void LaunchState::startTimeRollAnimation(const char* fromDigits, const char* toDigits) {
    // 动画只保存四个可见数字，避免在每一帧重新推导 RTC 时间并减少状态耦合。
    copyTimeDigits(timeRoll_.fromDigits, fromDigits);
    copyTimeDigits(timeRoll_.toDigits, toDigits);
    timeRoll_.startMs = millis();
    timeRoll_.durationMs = kTimeRollAnimationMs;
    if (timeRoll_.durationMs == 0) {
        timeRoll_.active = false;
        copyTimeDigits(visibleTimeDigits_, timeRoll_.toDigits);
        return;
    }
    timeRoll_.active = true;
}

void LaunchState::handleNtpSync(const AppMessage& msg) {
    rtc_.setTime(msg.ntpTime.year, msg.ntpTime.month, msg.ntpTime.day,
                 msg.ntpTime.hour, msg.ntpTime.minute, msg.ntpTime.second,
                 msg.ntpTime.weekday);

    RTCTime synced = {};
    synced.year = msg.ntpTime.year;
    synced.month = msg.ntpTime.month;
    synced.day = msg.ntpTime.day;
    synced.hour = msg.ntpTime.hour;
    synced.minute = msg.ntpTime.minute;
    synced.second = msg.ntpTime.second;
    synced.weekday = msg.ntpTime.weekday;
    applyTimeUpdate(synced);
    Serial.println("[Launch] NTP time written to RTC");
}
