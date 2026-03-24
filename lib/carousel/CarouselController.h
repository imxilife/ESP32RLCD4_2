#pragma once

#include "CardDescriptor.h"
#include <Animation.h>
#include <Font.h>

class Gui;

class CarouselController {
public:
    static constexpr int kMaxCards = 8;
    static constexpr uint32_t kDurationMs = 500;
    static constexpr float kSmallScale = 0.5f;
    static constexpr int kScreenW = 400;
    static constexpr int kScreenH = 300;
    static constexpr int kSlotSpacing = 250;

    void setCard(int index, const CardDescriptor& desc);
    void setCardCount(int count);
    void setTitleFont(const Font* font) { titleFont_ = font; }

    // 更新某张卡片的标题（如时间卡片动态更新）
    void updateCardTitle(int index, const char* title);

    // 从当前 activeIdx 动画切换到 toIdx
    void startTransition(int toIdx);
    bool isAnimating() const { return animating_; }

    // 推进动画并绘制所有可见卡片
    void tick(Gui& gui, uint32_t deltaMs);

    // 绘制静态布局（无动画时的当前状态）
    void drawStatic(Gui& gui);

    int activeIndex() const { return activeIdx_; }
    int cardCount() const { return cardCount_; }

private:
    struct CardPose {
        int cx, cy;
        float scale;
        int cardIdx;
    };

    CardDescriptor cards_[kMaxCards];
    int cardCount_ = 0;
    int activeIdx_ = 0;
    const Font* titleFont_ = nullptr;

    Animation anim_;
    int fromIdx_ = 0;
    int toIdx_ = 0;
    bool animating_ = false;

    // 计算所有卡片在当前动画进度下的位姿
    void computePoses(float progress, CardPose* poses, int& count);

    // 绘制单张卡片
    void drawCard(Gui& gui, const CardDescriptor& card, int cx, int cy, float scale);
};
