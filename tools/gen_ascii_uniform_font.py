#!/usr/bin/env python3
"""
生成统一字号/统一基线的 ASCII 1bpp 字体。

目的：
- 避免“每个字符单独缩放到满格”导致的字形失衡
- 保留同一字体族在小尺寸下的相对比例
- 用于嵌入式单色屏上的英文可读性测试
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ASCII_CHARS = ''.join(chr(i) for i in range(32, 127))
PROJECT_ROOT = Path(__file__).parent.parent
OUT_DIR = PROJECT_ROOT / "lib" / "ui" / "gui" / "fonts"


def to_1bpp(canvas_l, target_w, target_h, threshold):
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


def choose_font_size(font_path, target_w, target_h, chars, pad):
    lo, hi = 4, 80
    best = 4
    while lo <= hi:
        mid = (lo + hi) // 2
        font = ImageFont.truetype(str(font_path), mid)
        min_top = 0
        max_bottom = 0
        max_width = 0

        for ch in chars:
            bbox = font.getbbox(ch, anchor="ls")
            min_top = min(min_top, bbox[1])
            max_bottom = max(max_bottom, bbox[3])
            max_width = max(max_width, bbox[2] - bbox[0])

        family_h = max_bottom - min_top
        if family_h <= target_h - 2 * pad and max_width <= target_w - 2 * pad:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def render_uniform_chars(font_path, target_w, target_h, chars, pad=1, threshold=128):
    size = choose_font_size(font_path, target_w, target_h, chars, pad)
    font = ImageFont.truetype(str(font_path), size)

    min_top = 0
    max_bottom = 0
    for ch in chars:
        bbox = font.getbbox(ch, anchor="ls")
        min_top = min(min_top, bbox[1])
        max_bottom = max(max_bottom, bbox[3])

    family_h = max_bottom - min_top
    baseline_y = pad - min_top + (target_h - 2 * pad - family_h) // 2

    rendered = []
    for ch in chars:
        bbox = font.getbbox(ch, anchor="ls")
        glyph_w = bbox[2] - bbox[0]

        canvas = Image.new("L", (target_w, target_h), 0)
        draw = ImageDraw.Draw(canvas)

        draw_x = pad - bbox[0] + max(0, (target_w - 2 * pad - glyph_w) // 2)
        draw.text((draw_x, baseline_y), ch, font=font, fill=255, anchor="ls")

        rendered.append((ch, to_1bpp(canvas, target_w, target_h, threshold)))

    return rendered


def make_label(font_stem, target_w, target_h):
    safe = font_stem.replace("-", "_").replace(" ", "_")
    return f"Font_ascii_{safe}_{target_w}_{target_h}_Uniform"


def generate_font(font_path, target_w, target_h):
    chars = ASCII_CHARS
    data = render_uniform_chars(font_path, target_w, target_h, chars)

    label = make_label(font_path.stem, target_w, target_h)
    cpp_path = OUT_DIR / f"{label}.cpp"
    h_path = OUT_DIR / f"{label}.h"
    stride = (target_w + 7) // 8

    escaped = chars.replace("\\", "\\\\").replace('"', '\\"')
    cpp_lines = [
        "//",
        f"// 字体: {font_path.name}",
        "// 模式: ASCII Uniform",
        f"// 尺寸: {target_w}x{target_h}",
        "// 生成工具: gen_ascii_uniform_font.py",
        "//",
        "",
        f'#include "{h_path.stem}.h"',
        "#include <cstring>",
        "",
        "namespace {",
        f"static const int  k{label}_W      = {target_w};",
        f"static const int  k{label}_H      = {target_h};",
        f"static const int  k{label}_Stride = {stride};",
        f'static const char k{label}_Chars[] = "{escaped}";',
        f"static const uint8_t k{label}_Glyphs[{len(chars)}][{stride * target_h}] = {{",
    ]

    for ch, glyph in data:
        comment = ch if ch != "'" else "\\'"
        cpp_lines.append("    {" + ",".join(f"0x{b:02X}" for b in glyph) + f"}}, // '{comment}'")

    cpp_lines += [
        "};",
        "",
        "static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,",
        "                               int &stride, int &advX, const void*) {",
        "    if (cp > 127) return nullptr;",
        f"    const char* pos = strchr(k{label}_Chars, static_cast<char>(cp));",
        "    if (!pos) return nullptr;",
        f"    w = k{label}_W; h = k{label}_H;",
        f"    stride = k{label}_Stride; advX = k{label}_W;",
        f"    return k{label}_Glyphs[pos - k{label}_Chars];",
        "}",
        "} // namespace",
        "",
        f"const Font k{label} = {{",
        f"    /* lineHeight */ k{label}_H,",
        "    /* getGlyph   */ getGlyph,",
        "    /* data       */ nullptr,",
        "    /* fallback   */ nullptr,",
        "};",
    ]

    h_lines = [
        "//",
        f"// 字体: {font_path.name}",
        "// 模式: ASCII Uniform",
        f"// 尺寸: {target_w}x{target_h}",
        "//",
        "",
        "#pragma once",
        "",
        '#include "../Font.h"',
        "",
        f"extern const Font k{label};",
    ]

    cpp_path.write_text("\n".join(cpp_lines), encoding="utf-8")
    h_path.write_text("\n".join(h_lines), encoding="utf-8")
    return cpp_path, h_path


def main():
    targets = [
        (PROJECT_ROOT / "fonts" / "IBMPlexSans-Medium.ttf", 24, 24),
        (PROJECT_ROOT / "fonts" / "AlibabaPuHuiTi-3-75-SemiBold.ttf", 24, 24),
    ]
    for font_path, w, h in targets:
        cpp_path, h_path = generate_font(font_path, w, h)
        print(cpp_path)
        print(h_path)


if __name__ == "__main__":
    main()
