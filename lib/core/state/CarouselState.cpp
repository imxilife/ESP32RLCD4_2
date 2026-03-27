#include "CarouselState.h"
#include <Arduino.h>
#include <core/app_tasks/app_tasks.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <core/state_manager/StateId.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>

extern QueueHandle_t g_msgQueue;

CarouselState::CarouselState(Gui& gui)
    : gui_(gui) {}

void CarouselState::initCards() {
    carousel_.setCardCount(kCardCount);
    carousel_.setTitleFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    CardDescriptor cards[kCardCount] = {
        { timeStr_,     240, 140, 10 },  // 卡片0: 时间（动态标题）
        { "Pomodoro",   240, 140, 10 },  // 卡片1
        { "MusicPlay",  240, 140, 10 },  // 卡片2
        { "XZAI",       240, 140, 10 },  // 卡片3
        { "OTA",        240, 140, 10 },  // 卡片4
    };
    for (int i = 0; i < kCardCount; i++) {
        carousel_.setCard(i, cards[i]);
    }
}

void CarouselState::onEnter() {
    // 首次启动 RTC 后台任务
    static bool rtcStarted = false;
    if (!rtcStarted) {
        rtcStarted = true;
        xTaskCreate(rtcTask, "rtcTask", 2048, &rtc_, 2, nullptr);
        Serial.println("[Carousel] RTC task started");
    }

    initCards();
    lastTickMs_ = millis();
    carousel_.drawStatic(gui_);
}

void CarouselState::onExit() {
    // 目前 CarouselState 作为唯一主界面，不应退出
}

void CarouselState::onMessage(const AppMessage& msg) {
    if (msg.type == MSG_RTC_UPDATE) {
        updateTimeStr(msg.rtcTime.hour, msg.rtcTime.minute);

        // 更新卡片0的标题指针（指向 timeStr_ 缓冲区，始终有效）
        carousel_.updateCardTitle(0, timeStr_);

        // 如果当前不在动画中且时间卡片是活跃卡片，重绘
        if (!carousel_.isAnimating() && carousel_.activeIndex() == 0) {
            carousel_.drawStatic(gui_);
        }
    }
    // 其他消息类型暂时忽略
}

void CarouselState::onKeyEvent(const KeyEvent& event) {
    // 动画中吞掉所有按键
    if (carousel_.isAnimating()) return;

    if (event.id == KeyId::KEY1 && event.action == KeyAction::DOWN) {
        int nextIdx = (carousel_.activeIndex() + 1) % kCardCount;
        carousel_.startTransition(nextIdx);
        lastTickMs_ = millis();
    } else if (event.id == KeyId::KEY2 && event.action == KeyAction::DOWN) {
        requestTransition(stateForCard(carousel_.activeIndex()));
    }
}

void CarouselState::tick() {
    if (!carousel_.isAnimating()) return;

    uint32_t now = millis();
    uint32_t deltaMs = now - lastTickMs_;
    lastTickMs_ = now;

    carousel_.tick(gui_, deltaMs);

    // 动画刚结束，绘制最终静态布局
    if (!carousel_.isAnimating()) {
        carousel_.drawStatic(gui_);
    }
}

void CarouselState::updateTimeStr(uint8_t hour, uint8_t minute) {
    snprintf(timeStr_, sizeof(timeStr_), "%02d:%02d", hour, minute);
}

StateId CarouselState::stateForCard(int cardIndex) const {
    switch (cardIndex) {
    case 0:
        return StateId::MAIN_UI;
    case 1:
        return StateId::POMODORO;
    case 2:
        return StateId::MUSIC_PLAYER;
    case 3:
        return StateId::XZAI;
    case 4:
        return StateId::OTA;
    default:
        return StateId::CAROUSEL;
    }
}
