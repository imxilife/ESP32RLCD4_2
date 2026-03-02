#!/usr/bin/env python3
"""
Generate FontDigits glyphs.

大号 72x96  ─ Arial Black，冒号/小数点程序绘制
小号 24x32  ─ DIN Condensed Bold（瘦长），冒号/小数点程序绘制

修复说明：
  1&3. 扩大至 72x96 → 更多像素 → 曲线边缘更平滑
  2.   冒号用两个实心圆绘制，而非字体渲染的方块
  4.   小数点绘制在底部基线，实心圆
  5.   小号字体改为 DIN Condensed Bold，更瘦长
"""
import os
from PIL import Image, ImageDraw, ImageFont

BIG_W,   BIG_H   = 72, 96
SMALL_W, SMALL_H = 24, 32

BIG_FONT_PATH   = '/System/Library/Fonts/Supplemental/Arial Black.ttf'
SMALL_FONT_PATH = '/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf'
FALLBACK        = '/System/Library/Fonts/PingFang.ttc'

def resolve(path):
    return path if os.path.exists(path) else FALLBACK

chars = '0123456789:.'


# ───────────────────────────── 通用渲染 ─────────────────────────────

def to_1bpp(canvas_l, target_w, target_h, threshold):
    """将灰度 Image 二值化为 1bpp 数据列表"""
    stride = (target_w + 7) // 8
    data = []
    for row in range(target_h):
        for b_idx in range(stride):
            b = 0
            for bit in range(8):
                x = b_idx * 8 + bit
                if x < target_w and canvas_l.getpixel((x, row)) > threshold:
                    b |= 1 << (7 - bit)
            data.append(b)
    return data


def render_digit(ch, target_w, target_h, font_path, threshold=90):
    """
    通用字符渲染：4× 超采样 → LANCZOS 缩放 → 阈值二值化
    字形自动缩放充满 (target_w-4)×(target_h-4) 范围并居中
    """
    scale = 4
    pad   = 10 * scale

    font = ImageFont.truetype(font_path, target_h * scale)
    canvas_w = target_w * scale + pad * 2
    canvas_h = target_h * scale + pad * 2

    img = Image.new('L', (canvas_w, canvas_h), 0)
    ImageDraw.Draw(img).text((pad, pad), ch, font=font, fill=255)

    bbox = img.getbbox()
    if not bbox:
        return [0] * ((target_w + 7) // 8 * target_h)

    cropped  = img.crop(bbox)
    glyph_w, glyph_h = cropped.size

    avail_w = target_w - 4
    avail_h = target_h - 4
    ratio   = min(avail_w / glyph_w, avail_h / glyph_h)
    new_w   = max(1, round(glyph_w * ratio))
    new_h   = max(1, round(glyph_h * ratio))

    scaled = cropped.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new('L', (target_w, target_h), 0)
    ox = (target_w - new_w) // 2
    oy = (target_h - new_h) // 2
    canvas.paste(scaled, (max(0, ox), max(0, oy)))

    return to_1bpp(canvas, target_w, target_h, threshold)


# ─────────────────────── 特殊字符：程序绘制 ──────────────────────────

def draw_colon(target_w, target_h, dot_radius_frac=0.13, y1_frac=0.30, y2_frac=0.70):
    """
    两个实心圆的冒号。
    dot_radius_frac  = 圆半径 / target_w
    y1_frac / y2_frac = 两圆心 y / target_h
    """
    canvas = Image.new('L', (target_w, target_h), 0)
    d = ImageDraw.Draw(canvas)
    r  = max(2, round(target_w * dot_radius_frac))
    cx = target_w  // 2
    cy1 = round(target_h * y1_frac)
    cy2 = round(target_h * y2_frac)
    d.ellipse([cx - r, cy1 - r, cx + r, cy1 + r], fill=255)
    d.ellipse([cx - r, cy2 - r, cx + r, cy2 + r], fill=255)
    return to_1bpp(canvas, target_w, target_h, threshold=128)


def draw_dot(target_w, target_h, radius_frac=0.10, x_frac=0.50, bottom_margin=2):
    """
    小数点：底部基线，实心圆。
    x_frac        = 圆心 x / target_w（0.5 = 居中）
    bottom_margin = 距底边像素数
    """
    canvas = Image.new('L', (target_w, target_h), 0)
    d = ImageDraw.Draw(canvas)
    r  = max(2, round(target_w * radius_frac))
    cx = round(target_w * x_frac)
    cy = target_h - bottom_margin - r
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    return to_1bpp(canvas, target_w, target_h, threshold=128)


# ─────────────────────────── 主生成逻辑 ─────────────────────────────

def render_set(label, target_w, target_h, font_path, threshold=90,
               colon_r=0.13, dot_r=0.10):
    stride = (target_w + 7) // 8
    print(f"// {label} {target_w}x{target_h}")
    print(f"static const int k{label}_Width = {target_w};")
    print(f"static const int k{label}_Height = {target_h};")
    print(f"static const int k{label}_Stride = {stride};")
    print(f"static const uint8_t k{label}_Glyphs[12][{stride * target_h}] = {{")

    for ch in chars:
        if ch == ':':
            data = draw_colon(target_w, target_h, dot_radius_frac=colon_r)
        elif ch == '.':
            data = draw_dot(target_w, target_h, radius_frac=dot_r)
        else:
            data = render_digit(ch, target_w, target_h, font_path, threshold)
        print("    {" + ','.join(f'0x{b:02X}' for b in data) + f"}}, // '{ch}'")

    print("};")
    print()


# 大号 72×96：Arial Black，threshold=90
render_set('FontBig72x96',   BIG_W,   BIG_H,
           resolve(BIG_FONT_PATH),   threshold=90,
           colon_r=0.13, dot_r=0.11)

# 小号 24×32：DIN Condensed Bold（瘦长），threshold=90
render_set('FontSmall24x32', SMALL_W, SMALL_H,
           resolve(SMALL_FONT_PATH), threshold=90,
           colon_r=0.12, dot_r=0.10)
