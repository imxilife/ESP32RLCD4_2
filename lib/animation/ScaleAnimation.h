#pragma once

#include "Animation.h"

class ScaleAnimation : public Animation {
public:
    float fromScale = 1.0f;
    float toScale = 0.5f;

    float currentScale(float progress) const {
        return fromScale + (toScale - fromScale) * progress;
    }
};
