#pragma once

#include <stdint.h>
#include "Interpolator.h"

class Animation {
public:
    uint32_t durationMs = 500;
    Interpolator interpolator = kEaseInOut;

    void start() {
        elapsed_ = 0;
        running_ = true;
    }

    bool isRunning() const { return running_; }
    bool isFinished() const { return !running_; }

    // 推进动画，返回插值后的进度 [0.0, 1.0]
    float tick(uint32_t deltaMs) {
        if (!running_) return 1.0f;
        elapsed_ += deltaMs;
        if (elapsed_ >= durationMs) {
            running_ = false;
            elapsed_ = durationMs;
        }
        float t = (float)elapsed_ / (float)durationMs;
        return interpolator(t);
    }

protected:
    uint32_t elapsed_ = 0;
    bool running_ = false;
};
