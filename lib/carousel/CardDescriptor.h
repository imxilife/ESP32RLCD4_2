#pragma once

#include <stdint.h>

struct CardDescriptor {
    const char* title;
    uint16_t baseWidth;
    uint16_t baseHeight;
    uint8_t  cornerRadius;
};
