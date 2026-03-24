#pragma once

using Interpolator = float (*)(float t);

inline float kLinear(float t) { return t; }

inline float kEaseInOut(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float f = 2.0f * t - 2.0f;
    return 0.5f * f * f * f + 1.0f;
}
