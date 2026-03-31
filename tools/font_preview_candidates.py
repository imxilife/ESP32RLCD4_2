#!/usr/bin/env python3
"""
生成候选字体的 1bpp 预览图，便于在仓库里直接对比选择。

预览目标不是桌面排版效果，而是尽量模拟当前项目的字体生成流程：
- 先将 TTF/OTF 栅格化为灰度图
- 再缩放到目标像素尺寸
- 最后按阈值转成 1bpp
"""

from pathlib import Path
from PIL import Image, ImageDraw

from gen_font_digits import render_char

PROJECT_ROOT = Path(__file__).parent.parent
OUTPUT_DIR = PROJECT_ROOT / "docs" / "font_previews"


def glyph_to_image(data, w, h, scale=3):
    img = Image.new("L", (w * scale, h * scale), 255)
    d = ImageDraw.Draw(img)
    stride = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            byte_idx = y * stride + (x >> 3)
            bit_idx = 7 - (x & 7)
            if data[byte_idx] & (1 << bit_idx):
                x0 = x * scale
                y0 = y * scale
                d.rectangle([x0, y0, x0 + scale - 1, y0 + scale - 1], fill=0)
    return img


def make_sheet(title, entries, glyph_w, glyph_h, sample, output_name, scale=3, gap=8):
    rows = len(entries)
    cols = len(sample)
    label_w = 220
    cell_w = glyph_w * scale
    cell_h = glyph_h * scale
    title_h = 36
    row_h = cell_h + gap
    width = label_w + cols * (cell_w + gap) + gap
    height = title_h + rows * row_h + gap

    sheet = Image.new("L", (width, height), 255)
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 8), title, fill=0)

    for row, (label, font_path) in enumerate(entries):
        y = title_h + row * row_h
        draw.text((8, y + cell_h // 2 - 8), label, fill=0)
        for col, ch in enumerate(sample):
            x = label_w + col * (cell_w + gap)
            data = render_char(ch, glyph_w, glyph_h, str(font_path))
            glyph_img = glyph_to_image(data, glyph_w, glyph_h, scale=scale)
            sheet.paste(glyph_img, (x, y))
            draw.rectangle([x, y, x + cell_w - 1, y + cell_h - 1], outline=192)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUTPUT_DIR / output_name
    sheet.save(out)
    return out


def main():
    fonts_dir = PROJECT_ROOT / "fonts"

    digit_entries = [
        ("Current Geom Bold", fonts_dir / "Geom-Bold.ttf"),
        ("Rajdhani SemiBold", fonts_dir / "Rajdhani-SemiBold.otf"),
        ("IBM Plex Sans Condensed SemiBold", fonts_dir / "IBMPlexSansCondensed-SemiBold.ttf"),
    ]
    english_entries = [
        ("Current Oswald Light", fonts_dir / "Oswald-Light.ttf"),
        ("Atkinson Hyperlegible Regular", fonts_dir / "AtkinsonHyperlegible-Regular.ttf"),
        ("IBM Plex Sans Medium", fonts_dir / "IBMPlexSans-Medium.ttf"),
    ]
    chinese_entries = [
        ("Current Alibaba SemiBold", fonts_dir / "AlibabaPuHuiTi-3-75-SemiBold.ttf"),
        ("IBM Plex Sans SC Medium", fonts_dir / "IBMPlexSansSC-Medium.otf"),
    ]

    digit_samples = "0028465489"
    english_samples = "SATINFOKEY2"
    chinese_samples = "星期温湿年月日"

    outputs = [
        make_sheet("Digits Preview 40x64", digit_entries, 40, 64, digit_samples, "digits_40x64.png"),
        make_sheet("English Preview 20x20", english_entries, 20, 20, english_samples, "english_20x20.png"),
        make_sheet("Chinese Preview 20x20", chinese_entries, 20, 20, chinese_samples, "chinese_20x20.png"),
    ]

    print("Generated previews:")
    for out in outputs:
        print(out)


if __name__ == "__main__":
    main()
