#pragma once

#include "Animation.h"

class TranslateAnimation : public Animation {
public:
    int fromX = 0, fromY = 0;
    int toX = 0, toY = 0;

    int currentX(float progress) const {
        return fromX + (int)((toX - fromX) * progress);
    }

    int currentY(float progress) const {
        return fromY + (int)((toY - fromY) * progress);
    }
};
