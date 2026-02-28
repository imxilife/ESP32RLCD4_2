//
// Created by Kelly on 26-2-12.
//

#pragma once

#include "display_bsp.h"   // 里面有 DisplayPort 和 ColorBlack/ColorWhite

#ifndef ESP32_RLCD4_2_GUI_H
#define ESP32_RLCD4_2_GUI_H

/**
 * Gui
 *
 * 基于 DisplayPort 的单色图形库，接口风格参考 u8g2：
 * - 提供点、线、基本几何图形（矩形、圆、圆角矩形、三角形）绘制；
 * - 提供 ASCII 文本与 UTF-8 文本（含中文）的绘制；
 * - 对单色屏显式区分“前景色”和“背景色”，可按前景/背景覆盖绘制。
 *
 * 所有绘制操作只修改显示缓冲区，真正刷屏由 display() 触发。
 */
class Gui {
public:
    /// 外部中文点阵查找函数：输入 Unicode 码点，返回 1bpp 位图指针及尺寸信息
    typedef const uint8_t *(*ChineseGlyphProvider)(uint32_t codepoint,
                                                   int &width,
                                                   int &height,
                                                   int &strideBytes);

    Gui(DisplayPort *lcd, int width, int height);

    // === 帧缓冲与颜色控制 ===

    /// 使用指定颜色清屏（不立刻刷到 LCD）
    void clear(uint8_t color);

    /// 使用当前背景色清屏（不立刻刷到 LCD）
    void clear();

    /// 将缓冲区内容刷到 LCD
    void display();

    /// 设置当前前景色（逻辑上用于“画线/文字/描边”）
    void setForegroundColor(uint8_t color);

    /// 设置当前背景色（逻辑上用于“清屏/文字背景/填充区域”）
    void setBackgroundColor(uint8_t color);

    /// 获取当前前景色
    uint8_t foregroundColor() const;

    /// 获取当前背景色
    uint8_t backgroundColor() const;

    // === 基础绘图 ===

    /// 画一个像素点，坐标越界会被安全丢弃
    void drawPixel(int x, int y, uint8_t color);

    /// 使用 Bresenham 算法画直线
    void drawLine(int x0, int y0, int x1, int y1, uint8_t color);

    // === 几何图形 ===

    /// 画矩形边框
    void drawRect(int x, int y, int w, int h, uint8_t color);

    /// 画实心矩形
    void fillRect(int x, int y, int w, int h, uint8_t color);

    /// 画圆（边框）
    void drawCircle(int x0, int y0, int radius, uint8_t color);

    /// 画实心圆
    void fillCircle(int x0, int y0, int radius, uint8_t color);

    /// 画圆角矩形（边框）
    void drawRoundRect(int x, int y, int w, int h, int radius, uint8_t color);

    /// 画实心圆角矩形
    void fillRoundRect(int x, int y, int w, int h, int radius, uint8_t color);

    /// 画三角形边框
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color);

    /// 画实心三角形
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color);

    // === 文本（前景/背景色可选） ===

    /// 使用显式前景色（透明背景）画单个 ASCII 字符
    void drawChar(int x, int y, char c, uint8_t color);

    /// 使用当前前景/背景色画单个 ASCII 字符（会覆盖整个字符单元背景）
    void drawChar(int x, int y, char c);

    /// 使用显式前景/背景色画单个 ASCII 字符（会覆盖整个字符单元背景）
    void drawChar(int x, int y, char c, uint8_t fgColor, uint8_t bgColor);

    /// 使用显式前景色（透明背景）画 ASCII 字符串
    void drawString(int x, int y, const char *str, uint8_t color);

    /// 使用当前前景/背景色画 ASCII 字符串（会覆盖字符串区域背景）
    void drawString(int x, int y, const char *str);

    /// 使用显式前景/背景色画 ASCII 字符串（会覆盖字符串区域背景）
    void drawString(int x, int y, const char *str, uint8_t fgColor, uint8_t bgColor);

    /// 使用显式前景色（透明背景）画 UTF-8 字符串（含中文）
    void drawUTF8String(int x, int y, const char *utf8, uint8_t color);

    /// 使用当前前景/背景色画 UTF-8 字符串（会覆盖字符串区域背景）
    void drawUTF8String(int x, int y, const char *utf8);

    /// 使用显式前景/背景色画 UTF-8 字符串（会覆盖字符串区域背景）
    void drawUTF8String(int x, int y, const char *utf8, uint8_t fgColor, uint8_t bgColor);

    // === 位图 ===

    /// 画 1bpp 位图（按行从左到右，高位在左），color 表示点亮像素的颜色
    void drawBitmap(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color);

    // === 中文点阵 ===

    /// 配置中文点阵提供回调，用于从外部字库取出 1bpp 点阵
    void setChineseGlyphProvider(ChineseGlyphProvider provider);

private:
    DisplayPort *lcd_;
    int width_;
    int height_;

    uint8_t fgColor_;  ///< 当前前景色
    uint8_t bgColor_;  ///< 当前背景色

    ChineseGlyphProvider chineseGlyphProvider_;
};

#endif //ESP32_RLCD4_2_GUI_H

//
// Created by Kelly on 26-2-12.
//
#pragma once
#include "display_bsp.h"   // 里面有 DisplayPort 和 ColorBlack/ColorWhite

#ifndef ESP32_RLCD4_2_GUI_H
#define ESP32_RLCD4_2_GUI_H

class Gui {
public:
    Gui(DisplayPort *lcd, int width, int height);

    void clear(uint8_t color);                     // 清屏（只改缓冲区，不立刻刷 LCD）
    void drawPixel(int x, int y, uint8_t color);   // 画点
    void drawLine(int x0, int y0, int x1, int y1, uint8_t color);  // 画线
    void drawChar(int x, int y, char c, uint8_t color);            // 画单个字符
    void drawString(int x, int y, const char *str, uint8_t color); // 画字符串
    void drawBitmap(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color); // 显示位图

    void display();   // 调用 LCD 的 RLCD_Display，把缓冲区真正刷到屏幕

private:
    DisplayPort *lcd_;
    int width_;
    int height_;
};

#endif //ESP32_RLCD4_2_GUI_H
