#include "CarouselController.h"
#include <Gui.h>
#include <cmath>
#include <algorithm>

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
    fromIdx_ = activeIdx_;
    toIdx_ = toIdx;
    anim_.durationMs = kDurationMs;
    anim_.interpolator = kEaseInOut;
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

    // 从远到近绘制（scale 小的先画），确保居中卡片在最上层
    // 简单排序：先画 scale 小的
    for (int i = 0; i < poseCount - 1; i++) {
        for (int j = i + 1; j < poseCount; j++) {
            if (poses[j].scale < poses[i].scale) {
                CardPose tmp = poses[i];
                poses[i] = poses[j];
                poses[j] = tmp;
            }
        }
    }

    for (int i = 0; i < poseCount; i++) {
        drawCard(gui, cards_[poses[i].cardIdx],
                 poses[i].cx, poses[i].cy, poses[i].scale);
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

    // 从远到近绘制
    for (int i = 0; i < poseCount - 1; i++) {
        for (int j = i + 1; j < poseCount; j++) {
            if (poses[j].scale < poses[i].scale) {
                CardPose tmp = poses[i];
                poses[i] = poses[j];
                poses[j] = tmp;
            }
        }
    }

    for (int i = 0; i < poseCount; i++) {
        drawCard(gui, cards_[poses[i].cardIdx],
                 poses[i].cx, poses[i].cy, poses[i].scale);
    }
}

void CarouselController::computePoses(float progress, CardPose* poses, int& count) {
    count = 0;
    if (cardCount_ == 0) return;

    int centerX = kScreenW / 2;
    int centerY = kScreenH / 2;

    for (int i = 0; i < cardCount_; i++) {
        // 相对于 fromIdx 的槽位偏移
        int slotOffset = i - fromIdx_;

        // 动画进度使所有槽向左移动
        float visualOffset = (float)slotOffset - progress;

        int cx = centerX + (int)(visualOffset * kSlotSpacing);
        float absOffset = fabsf(visualOffset);

        // 缩放：中心为 1.0，每偏移一个槽位缩小到 kSmallScale
        float scale = 1.0f - absOffset * (1.0f - kSmallScale);
        if (scale < kSmallScale) scale = kSmallScale;
        if (scale > 1.0f) scale = 1.0f;

        // 计算卡片缩放后的宽度，用于裁剪判断
        int sw = (int)(cards_[i].baseWidth * scale);
        int leftEdge = cx - sw / 2;
        int rightEdge = cx + sw / 2;

        // 完全在屏幕外的卡片跳过
        if (rightEdge < 0 || leftEdge >= kScreenW) continue;

        poses[count].cx = cx;
        poses[count].cy = centerY;
        poses[count].scale = scale;
        poses[count].cardIdx = i;
        count++;
    }
}

void CarouselController::drawCard(Gui& gui, const CardDescriptor& card,
                                   int cx, int cy, float scale) {
    int sw = (int)(card.baseWidth * scale);
    int sh = (int)(card.baseHeight * scale);
    int sx = cx - sw / 2;
    int sy = cy - sh / 2;

    // 二次裁剪检查
    if (sx + sw < 0 || sx >= kScreenW || sy + sh < 0 || sy >= kScreenH) return;

    int sr = std::max(1, (int)(card.cornerRadius * scale));

    gui.fillRoundRect(sx, sy, sw, sh, sr, ColorWhite);
    gui.drawRoundRect(sx, sy, sw, sh, sr, ColorBlack);

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
        gui.drawText(tx, ty, card.title, ColorBlack, ColorWhite);
    }
}
