#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>
#include <device/rtc/RTC85063.h>
#include <ui/carousel/CarouselController.h>

class CarouselState : public AbstractState {
public:
    explicit CarouselState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;
    void tick()                            override;

private:
    StateId stateForCard(int cardIndex) const;

    Gui& gui_;
    CarouselController carousel_;
    RTC85063 rtc_;

    // 时间卡片的动态标题缓存
    char timeStr_[6] = "00:00";

    uint32_t lastTickMs_ = 0;

    static constexpr int kCardCount = 5;
    // 卡片索引：0=时间, 1=Pomodoro, 2=MusicPlay, 3=XZAI, 4=OTA

    void initCards();
    void updateTimeStr(uint8_t hour, uint8_t minute);
};
