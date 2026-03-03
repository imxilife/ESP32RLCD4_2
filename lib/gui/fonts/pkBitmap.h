//
// Created by Kelly on 26-3-4.
// 单色 1bpp 位图资源，配合 Gui::drawBitmap 使用
//

#ifndef ESP32RLCD4_2_PKBITMAP_H
#define ESP32RLCD4_2_PKBITMAP_H

#include <stdint.h>

// 位图尺寸（像素）：宽 280 x 高 240
// 8400 字节 = 280 * 240 / 8
#define PK_BITMAP_WIDTH   223
#define PK_BITMAP_HEIGHT  300

// 1bpp 位图数据，按行从左到右，高位在左
extern const uint8_t gImage_out[8400];

#endif //ESP32RLCD4_2_PKBITMAP_H
