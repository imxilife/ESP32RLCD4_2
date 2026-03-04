#!/usr/bin/env python3
"""
字体转换工具 - 将TTF/OTF字体转换为C数组

功能：
  - 扫描项目根目录下的所有ttf/otf字体文件
  - 手动设置字体大小（宽x高）
  - 批量为所有找到的字体生成C数组文件
  - 自动输出到 lib/gui/fonts/ 目录
  - 文件命名格式：FontXX_字体名.cpp（XX 为像素高度）
  - 默认支持数字、冒号、小数点字符（可扩展）
"""
import os
import sys
import glob
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# 默认字符集（数字、冒号、小数点）
DEFAULT_CHARS = '0123456789:.'

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent.absolute()
FONTS_OUTPUT_DIR = PROJECT_ROOT / 'lib' / 'gui' / 'fonts'


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

def generate_font_cpp(font_path, font_size, output_path, chars=DEFAULT_CHARS, 
                     threshold=90, colon_r=0.13, dot_r=0.10):
    """
    生成字体C++文件
    
    Args:
        font_path: 字体文件路径
        font_size: 字体大小 (宽度, 高度)
        output_path: 输出文件路径
        chars: 要生成的字符集
        threshold: 二值化阈值
        colon_r: 冒号圆点半径比例
        dot_r: 小数点半径比例
    """
    target_w, target_h = font_size
    font_name = Path(font_path).stem
    # 约定：FontXX_字体名，其中 XX 使用像素高度，便于快速按高度区分
    label = f"Font{target_h}_{font_name.replace('-', '_').replace(' ', '_')}"
    
    stride = (target_w + 7) // 8
    
    # 生成C++代码
    cpp_code = []
    cpp_code.append(f"//")
    cpp_code.append(f"// 字体: {font_name}")
    cpp_code.append(f"// 尺寸: {target_w}x{target_h}")
    cpp_code.append(f"// 字符集: {chars}")
    cpp_code.append(f"// 生成工具: gen_font_digits.py")
    cpp_code.append(f"//")
    cpp_code.append(f"")
    cpp_code.append(f'#include "{Path(output_path).stem}.h"')
    cpp_code.append(f"")
    cpp_code.append(f"namespace {{")
    cpp_code.append(f"")
    cpp_code.append(f"// {label} {target_w}x{target_h}")
    cpp_code.append(f"static const int k{label}_Width = {target_w};")
    cpp_code.append(f"static const int k{label}_Height = {target_h};")
    cpp_code.append(f"static const int k{label}_Stride = {stride};")
    cpp_code.append(f"static const uint8_t k{label}_Glyphs[{len(chars)}][{stride * target_h}] = {{")
    
    for ch in chars:
        if ch == ':':
            data = draw_colon(target_w, target_h, dot_radius_frac=colon_r)
        elif ch == '.':
            data = draw_dot(target_w, target_h, radius_frac=dot_r)
        else:
            data = render_digit(ch, target_w, target_h, font_path, threshold)
        cpp_code.append("    {" + ','.join(f'0x{b:02X}' for b in data) + f"}}, // '{ch}'")
    
    cpp_code.append("};")
    cpp_code.append("")
    cpp_code.append("} // namespace")
    cpp_code.append("")
    cpp_code.append(f"// 导出接口")
    cpp_code.append(f"const uint8_t* get{label}_Glyph(char ch) {{")
    cpp_code.append(f"    const char* pos = strchr(\"{chars}\", ch);")
    cpp_code.append(f"    if (!pos) return nullptr;")
    cpp_code.append(f"    return k{label}_Glyphs[pos - \"{chars}\"];")
    cpp_code.append(f"}}")
    cpp_code.append("")
    cpp_code.append(f"int get{label}_Width() {{ return k{label}_Width; }}")
    cpp_code.append(f"int get{label}_Height() {{ return k{label}_Height; }}")
    cpp_code.append(f"int get{label}_Stride() {{ return k{label}_Stride; }}")
    
    # 写入文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(cpp_code))
    
    # 生成头文件
    h_path = output_path.with_suffix('.h')
    h_code = []
    h_code.append(f"//")
    h_code.append(f"// 字体: {font_name}")
    h_code.append(f"// 尺寸: {target_w}x{target_h}")
    h_code.append(f"//")
    h_code.append(f"")
    h_code.append(f"#pragma once")
    h_code.append(f"")
    h_code.append(f"#include <cstdint>")
    h_code.append(f"#include <cstring>")
    h_code.append(f"")
    h_code.append(f"// 获取字符点阵数据")
    h_code.append(f"const uint8_t* get{label}_Glyph(char ch);")
    h_code.append(f"")
    h_code.append(f"// 获取字体尺寸")
    h_code.append(f"int get{label}_Width();")
    h_code.append(f"int get{label}_Height();")
    h_code.append(f"int get{label}_Stride();")
    
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(h_code))
    
    print(f"✓ 生成成功: {output_path.name}")
    print(f"  - 字体: {font_name}")
    print(f"  - 尺寸: {target_w}x{target_h}")
    print(f"  - 字符数: {len(chars)}")
    print()


def find_fonts():
    """查找项目根目录下的所有字体文件"""
    fonts = []
    for ext in ['*.ttf', '*.otf', '*.TTF', '*.OTF']:
        fonts.extend(glob.glob(str(PROJECT_ROOT / ext)))
    return sorted(fonts)


def main():
    print("=" * 60)
    print("字体转换工具 - TTF/OTF 转 C数组")
    print("=" * 60)
    print()

    # 查找字体文件
    fonts = find_fonts()

    if not fonts:
        print("❌ 未在项目根目录找到字体文件（ttf/otf）")
        print(f"   项目根目录: {PROJECT_ROOT}")
        return

    print(f"找到 {len(fonts)} 个字体文件:")
    for i, font in enumerate(fonts, 1):
        print(f"  {i}. {Path(font).name}")
    print()

    # 输入字体尺寸（对所有字体统一使用）
    print("请输入字体尺寸（宽度x高度）")
    print("示例: 72x96, 24x32, 48x64")

    while True:
        try:
            size_input = input("字体尺寸: ").strip()
            if 'x' in size_input or 'X' in size_input:
                w, h = size_input.lower().split('x')
                width = int(w.strip())
                height = int(h.strip())
                if width > 0 and height > 0 and width <= 200 and height <= 200:
                    break
                else:
                    print("❌ 尺寸范围应在 1-200 之间")
            else:
                print("❌ 格式错误，请使用 宽度x高度 格式，如: 72x96")
        except ValueError:
            print("❌ 请输入有效的数字")
        except KeyboardInterrupt:
            print("\n取消操作")
            return

    print()

    # 确认字符集
    print(f"默认字符集: {DEFAULT_CHARS}")
    custom_chars = input("使用自定义字符集？(直接回车使用默认): ").strip()
    chars = custom_chars if custom_chars else DEFAULT_CHARS

    print()
    print("-" * 60)
    print("配置信息:")
    print(f"  字体数量: {len(fonts)}")
    print(f"  字体尺寸: {width}x{height}")
    print(f"  字符集: {chars}")
    print(f"  输出目录: {FONTS_OUTPUT_DIR}")
    print("-" * 60)
    print("将为以下字体生成位图:")
    for font in fonts:
        print(f"  - {Path(font).name}")
    print("-" * 60)
    print()

    # 确认生成
    confirm = input("确认为以上所有字体生成？(y/n): ").strip().lower()
    if confirm not in ['y', 'yes', '是']:
        print("取消操作")
        return

    print()

    # 确保输出目录存在
    FONTS_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # 逐个字体生成
    success_count = 0
    for font_path in fonts:
        font_name = Path(font_path).stem
        output_filename = f"Font{height}_{font_name.replace('-', '_').replace(' ', '_')}.cpp"
        output_path = FONTS_OUTPUT_DIR / output_filename

        try:
            generate_font_cpp(
                font_path,
                (width, height),
                output_path,
                chars=chars
            )
            success_count += 1
        except Exception as e:
            print(f"❌ 生成失败（{Path(font_path).name}）: {e}")
            import traceback
            traceback.print_exc()

    print("=" * 60)
    print(f"✓ 完成！成功生成 {success_count}/{len(fonts)} 个字体文件")
    print("=" * 60)


if __name__ == '__main__':
    main()
