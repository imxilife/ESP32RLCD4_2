#!/usr/bin/env python3
"""
生成可变宽 ASCII 1bpp 字体。

特点：
- 同一字体共用字号与基线
- 每个字符保留自身 advance 宽度
- 适合小尺寸英文显示对比，不再强制塞成固定方格
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


def render_chars(font_path, font_size, target_h, chars, threshold, side_pad=1):
    font = ImageFont.truetype(str(font_path), font_size)
    rendered = []

    for ch in chars:
        bbox = font.getbbox(ch, anchor="ls")
        adv = max(1, round(font.getlength(ch)))
        glyph_w = max(1, bbox[2] - bbox[0])
        width = max(adv, glyph_w) + side_pad * 2
        baseline = target_h - 6

        canvas = Image.new("L", (width, target_h), 0)
        draw = ImageDraw.Draw(canvas)
        draw_x = side_pad - bbox[0]
        draw.text((draw_x, baseline), ch, font=font, fill=255, anchor="ls")

        rendered.append({
            "char": ch,
            "width": width,
            "height": target_h,
            "stride": (width + 7) // 8,
            "advance": width,
            "data": to_1bpp(canvas, width, target_h, threshold),
        })

    return rendered


def make_label(font_stem, target_h):
    safe = font_stem.replace("-", "_").replace(" ", "_")
    return f"Font_ascii_{safe}_{target_h}_Var"


def generate_font(font_path, font_size, target_h, threshold):
    glyphs = render_chars(font_path, font_size, target_h, ASCII_CHARS, threshold)
    label = make_label(font_path.stem, target_h)
    cpp_path = OUT_DIR / f"{label}.cpp"
    h_path = OUT_DIR / f"{label}.h"

    escaped_chars = ASCII_CHARS.replace("\\", "\\\\").replace('"', '\\"')

    bitmap_data = []
    entries = []
    offset = 0
    for glyph in glyphs:
        entries.append({
            "char": glyph["char"],
            "codepoint": ord(glyph["char"]),
            "width": glyph["width"],
            "height": glyph["height"],
            "stride": glyph["stride"],
            "advance": glyph["advance"],
            "offset": offset,
        })
        bitmap_data.extend(glyph["data"])
        offset += len(glyph["data"])

    cpp_lines = [
        "//",
        f"// 字体: {font_path.name}",
        "// 模式: ASCII Variable Width",
        f"// 行高: {target_h}",
        "// 生成工具: gen_ascii_varwidth_font.py",
        "//",
        "",
        f'#include "{h_path.stem}.h"',
        "#include <cstring>",
        "",
        "namespace {",
        "struct GlyphEntry {",
        "    uint32_t codepoint;",
        "    uint8_t width;",
        "    uint8_t height;",
        "    uint8_t stride;",
        "    uint8_t advance;",
        "    uint16_t offset;",
        "};",
        "",
        f'static const char k{label}_Chars[] = "{escaped_chars}";',
        f"static const uint8_t k{label}_Bitmap[] = {{",
    ]

    row = []
    for i, b in enumerate(bitmap_data):
        row.append(f"0x{b:02X}")
        if len(row) == 24:
            cpp_lines.append("    " + ",".join(row) + ",")
            row = []
    if row:
        cpp_lines.append("    " + ",".join(row) + ",")

    cpp_lines += [
        "};",
        "",
        f"static const GlyphEntry k{label}_Glyphs[] = {{",
    ]

    for entry in entries:
        ch = entry["char"]
        comment = "\\'" if ch == "'" else ("space" if ch == " " else ch)
        cpp_lines.append(
            f"    {{0x{entry['codepoint']:04X}, {entry['width']}, {entry['height']}, "
            f"{entry['stride']}, {entry['advance']}, {entry['offset']}}}, // {comment}"
        )

    cpp_lines += [
        "};",
        "",
        f"static const int k{label}_Count = sizeof(k{label}_Glyphs) / sizeof(k{label}_Glyphs[0]);",
        "",
        "static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,",
        "                               int &stride, int &advX, const void*) {",
        f"    for (int i = 0; i < k{label}_Count; ++i) {{",
        f"        if (k{label}_Glyphs[i].codepoint == cp) {{",
        f"            w = k{label}_Glyphs[i].width;",
        f"            h = k{label}_Glyphs[i].height;",
        f"            stride = k{label}_Glyphs[i].stride;",
        f"            advX = k{label}_Glyphs[i].advance;",
        f"            return &k{label}_Bitmap[k{label}_Glyphs[i].offset];",
        "        }",
        "    }",
        "    return nullptr;",
        "}",
        "} // namespace",
        "",
        f"const Font k{label} = {{",
        f"    /* lineHeight */ {target_h},",
        "    /* getGlyph   */ getGlyph,",
        "    /* data       */ nullptr,",
        "    /* fallback   */ nullptr,",
        "};",
    ]

    h_lines = [
        "//",
        f"// 字体: {font_path.name}",
        "// 模式: ASCII Variable Width",
        f"// 行高: {target_h}",
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
    configs = [
        (PROJECT_ROOT / "fonts" / "IBMPlexSans-Medium.ttf", 23, 24, 150),
        (PROJECT_ROOT / "fonts" / "AlibabaPuHuiTi-3-75-SemiBold.ttf", 21, 24, 140),
    ]
    for font_path, font_size, target_h, threshold in configs:
        cpp_path, h_path = generate_font(font_path, font_size, target_h, threshold)
        print(cpp_path)
        print(h_path)


if __name__ == "__main__":
    main()
