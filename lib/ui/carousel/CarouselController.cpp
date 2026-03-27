#include "CarouselController.h"
#include <ui/gui/Gui.h>
#include <algorithm>

namespace {
inline float easeInCubic(float t) {
    return t * t * t;
}

inline float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - (u * u * u);
}
}  // namespace

void CarouselController::setCard(int index, const CardDescriptor& desc) {
    if (index >= 0 && index < kMaxCards) {
        cards_[index] = desc;
    }
}

void CarouselController::setCardCount(int count) {
    cardCount_ = std::min(count, kMaxCards);
}

void CarouselController::updateCardTitle(int index, const char* title) {
    if (index >= 0 && index < cardCount_) {
        cards_[index].title = title;
    }
}

void CarouselController::startTransition(int toIdx) {
    if (animating_ || cardCount_ == 0) return;
    if (toIdx < 0 || toIdx >= cardCount_) return;
    if (toIdx == activeIdx_) return;

    fromIdx_ = activeIdx_;
    toIdx_ = toIdx;
    anim_.durationMs = kDurationMs;
    // 使用线性时间轴，分别对出场和入场应用不同 easing 曲线
    anim_.interpolator = kLinear;
    anim_.start();
    animating_ = true;
}

void CarouselController::tick(Gui& gui, uint32_t deltaMs) {
    if (!animating_) return;

    float progress = anim_.tick(deltaMs);

    // 清屏
    gui.clear();

    // 计算并绘制所有可见卡片
    CardPose poses[kMaxCards];
    int poseCount = 0;
    computePoses(progress, poses, poseCount);

    // 固定绘制顺序：先出场卡，再入场卡，视觉上更连贯
    for (int i = 0; i < poseCount; i++) {
        drawCard(gui, cards_[poses[i].cardIdx], poses[i].cx, poses[i].cy);
    }

    // 动画结束
    if (anim_.isFinished()) {
        animating_ = false;
        activeIdx_ = toIdx_;
    }
}

void CarouselController::drawStatic(Gui& gui) {
    gui.clear();

    CardPose poses[kMaxCards];
    int poseCount = 0;
    computePoses(0.0f, poses, poseCount);

    for (int i = 0; i < poseCount; i++) {
        drawCard(gui, cards_[poses[i].cardIdx], poses[i].cx, poses[i].cy);
    }
}

void CarouselController::computePoses(float progress, CardPose* poses, int& count) {
    count = 0;
    if (cardCount_ == 0) return;

    int centerX = kScreenW / 2;
    int centerY = kScreenH / 2;
    if (!animating_) {
        poses[count].cx = centerX;
        poses[count].cy = centerY;
        poses[count].cardIdx = activeIdx_;
        count++;
        return;
    }

    // 出场：当前卡加速向左
    float outT = easeInCubic(progress);
    int fromCx = centerX - (int)(outT * kSlotSpacing);

    poses[count].cx = fromCx;
    poses[count].cy = centerY;
    poses[count].cardIdx = fromIdx_;
    count++;

    // 入场：目标卡从右向中间减速
    float inT = easeOutCubic(progress);
    int toCx = centerX + (int)((1.0f - inT) * kSlotSpacing);

    poses[count].cx = toCx;
    poses[count].cy = centerY;
    poses[count].cardIdx = toIdx_;
    count++;
}

void CarouselController::drawCard(Gui& gui, const CardDescriptor& card,
                                   int cx, int cy) {
    int sw = card.baseWidth;
    int sh = card.baseHeight;
    int sx = cx - sw / 2;
    int sy = cy - sh / 2;

    // 二次裁剪检查
    if (sx + sw < 0 || sx >= kScreenW || sy + sh < 0 || sy >= kScreenH) return;

    int sr = std::max(1, (int)(card.cornerRadius));
    bool isActiveCard = (!animating_) && (&card == &cards_[activeIdx_]);
    uint8_t bgColor = isActiveCard ? ColorBlack : ColorWhite;
    uint8_t fgColor = isActiveCard ? ColorWhite : ColorBlack;

    gui.fillRoundRect(sx, sy, sw, sh, sr, bgColor);
    gui.drawRoundRect(sx, sy, sw, sh, sr, fgColor);

    // 居中绘制标题
    if (card.title && titleFont_) {
        // 计算文字宽度
        int textW = 0;
        const char* p = card.title;
        while (*p) {
            uint32_t cp = 0;
            if ((*p & 0x80) == 0) {
                cp = *p++;
            } else if ((*p & 0xE0) == 0xC0) {
                cp = (*p & 0x1F) << 6; p++;
                cp |= (*p & 0x3F); p++;
            } else if ((*p & 0xF0) == 0xE0) {
                cp = (*p & 0x0F) << 12; p++;
                cp |= (*p & 0x3F) << 6; p++;
                cp |= (*p & 0x3F); p++;
            } else {
                p++; continue;
            }

            int gw, gh, stride, advance;
            const Font* f = titleFont_;
            while (f) {
                if (f->getGlyph(cp, gw, gh, stride, advance, f->data)) {
                    textW += advance;
                    break;
                }
                f = f->fallback;
            }
        }

        int textH = titleFont_->lineHeight;
        int tx = cx - textW / 2;
        int ty = cy - textH / 2;

        // 限制文字在卡片内
        if (tx < sx + 2) tx = sx + 2;
        if (ty < sy + 2) ty = sy + 2;

        gui.setFont(titleFont_);
        gui.drawText(tx, ty, card.title, fgColor, bgColor);
    }
}
