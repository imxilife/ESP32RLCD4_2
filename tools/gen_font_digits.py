#!/usr/bin/env python3
"""
字体转换工具 - 将TTF/OTF字体转换为C数组

使用流程：
  1. 选择字模类型：1-ASCII  2-中文
  2. 从项目根目录的字体文件中选择一个
  3. 输入字模宽高（支持 12x12 / 12X12 / 12*12）
  4. 生成 C++ 字模文件，输出到 lib/gui/fonts/
"""
import os
import sys
import glob
import re
import unicodedata
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# 默认 ASCII 字符集：可打印字符 32~126（空格 到 ~，共 95 个）
ASCII_DEFAULT_CHARS = ''.join(chr(i) for i in range(32, 127))

PROJECT_ROOT      = Path(__file__).parent.parent.absolute()
# 当前项目字体资源位于 lib/ui/gui/fonts，生成结果默认直接落到这里。
FONTS_OUTPUT_DIR  = PROJECT_ROOT / 'lib' / 'ui' / 'gui' / 'fonts'


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


def render_char(ch, target_w, target_h, font_path, threshold=70):
    """
    通用字符渲染：4× 超采样 → LANCZOS 缩放 → 阈值二值化
    字形自动缩放充满 (target_w-4)×(target_h-4) 范围并居中。
    宽度超出 target_w 时用负 ox 传入 paste，PIL 自动两侧对称裁剪。
    threshold=70（而非 90）保证曲线字形顶/底抗锯齿像素不被截掉，确保视觉等高。
    """
    scale = 4
    pad   = 10 * scale

    font     = ImageFont.truetype(font_path, target_h * scale)
    canvas_w = target_w * scale + pad * 2
    canvas_h = target_h * scale + pad * 2

    img = Image.new('L', (canvas_w, canvas_h), 0)
    ImageDraw.Draw(img).text((pad, pad), ch, font=font, fill=255)

    bbox = img.getbbox()
    if not bbox:
        return [0] * ((target_w + 7) // 8 * target_h)

    cropped        = img.crop(bbox)
    glyph_w, glyph_h = cropped.size

    avail_w = target_w - 4
    avail_h = target_h - 4
    # 同时约束宽高，避免宽字形（如 0/4/6/8/9）为了撑满高度而被左右裁切。
    ratio = min(avail_w / glyph_w, avail_h / glyph_h)
    new_w = max(1, round(glyph_w * ratio))
    new_h = max(1, round(glyph_h * ratio))

    scaled = cropped.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new('L', (target_w, target_h), 0)
    ox = (target_w - new_w) // 2
    oy = (target_h - new_h) // 2
    # 负 ox 时 PIL 会对称裁剪左右两侧，避免只裁右边
    canvas.paste(scaled, (ox, max(0, oy)))

    return to_1bpp(canvas, target_w, target_h, threshold)


# ─────────────────────── 特殊字符：程序绘制 ──────────────────────────

def draw_colon(target_w, target_h, dot_radius_frac=0.13, y1_frac=0.30, y2_frac=0.70):
    """两个实心圆的冒号"""
    canvas = Image.new('L', (target_w, target_h), 0)
    d  = ImageDraw.Draw(canvas)
    r  = max(2, round(target_w * dot_radius_frac))
    cx = target_w // 2
    cy1 = round(target_h * y1_frac)
    cy2 = round(target_h * y2_frac)
    d.ellipse([cx - r, cy1 - r, cx + r, cy1 + r], fill=255)
    d.ellipse([cx - r, cy2 - r, cx + r, cy2 + r], fill=255)
    return to_1bpp(canvas, target_w, target_h, threshold=128)


def draw_dot(target_w, target_h, radius_frac=0.10, x_frac=0.50, bottom_margin=2):
    """小数点：底部基线，实心圆"""
    canvas = Image.new('L', (target_w, target_h), 0)
    d  = ImageDraw.Draw(canvas)
    r  = max(2, round(target_w * radius_frac))
    cx = round(target_w * x_frac)
    cy = target_h - bottom_margin - r
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    return to_1bpp(canvas, target_w, target_h, threshold=128)


# ─────────────────────────── 字模预览 ─────────────────────────────

PIXEL_ON  = '█'
PIXEL_OFF = ' '


def glyph_to_lines(data, target_w, target_h):
    """将 1bpp 字模数据解码为文本行列表（每行一个字符串）"""
    stride = (target_w + 7) // 8
    lines = []
    for row in range(target_h):
        line = ''
        for col in range(target_w):
            byte_idx = row * stride + (col >> 3)
            bit_idx  = 7 - (col & 7)
            line += PIXEL_ON if (data[byte_idx] & (1 << bit_idx)) else PIXEL_OFF
        lines.append(line)
    return lines


def print_glyph_preview(char_data_list, target_w, target_h):
    """将所有字模以字符画方式打印，自动按终端宽度分列"""
    try:
        term_w = os.get_terminal_size().columns
    except OSError:
        term_w = 100

    # 每个字模显示宽度：左右竖线 + 像素列
    cell_w    = target_w + 2
    cols_per_row = max(1, term_w // cell_w)

    print()
    print("─" * min(term_w, cols_per_row * cell_w))
    print("字模预览")
    print("─" * min(term_w, cols_per_row * cell_w))

    for batch_start in range(0, len(char_data_list), cols_per_row):
        batch = char_data_list[batch_start:batch_start + cols_per_row]

        # 字符标题行
        header = ''
        for ch, _ in batch:
            label = f"'{ch}'"
            header += label.center(cell_w)
        print(header)

        # 像素行
        glyph_lines = [glyph_to_lines(data, target_w, target_h) for _, data in batch]
        for row in range(target_h):
            line = ''
            for col_idx in range(len(batch)):
                line += '|' + glyph_lines[col_idx][row] + '|'
            print(line)

        print()


# ─────────────────────────── 用户交互 ─────────────────────────────

def _wcwidth(ch):
    """返回字符在终端中占用的列数：宽字符（CJK 等）返回 2，其余返回 1"""
    return 2 if unicodedata.east_asian_width(ch) in ('W', 'F') else 1


def _read_exact(fd, n):
    """精确读取 n 个字节（防止 os.read 短读）"""
    buf = b''
    while len(buf) < n:
        chunk = os.read(fd, n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf


def ask(prompt):
    """读取用户输入，正确处理宽字符退格；单独输入 q/Q 回车则退出。
    在支持 tty 的平台（macOS/Linux）使用自定义逐字符读取，
    否则回退到标准 input()。
    """
    try:
        import tty
        import termios
    except ImportError:
        # Windows 等不支持 tty 的平台，回退
        try:
            result = input(prompt).strip()
        except KeyboardInterrupt:
            print("\n退出")
            sys.exit(0)
        if result.lower() == 'q':
            print("退出")
            sys.exit(0)
        return result

    sys.stdout.write(prompt)
    sys.stdout.flush()

    chars = []
    fd  = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while True:
            b0 = os.read(fd, 1)
            if not b0:
                continue
            byte = b0[0]

            if byte in (10, 13):        # Enter
                sys.stdout.write('\n')
                sys.stdout.flush()
                break

            if byte in (127, 8):        # Backspace / DEL
                if chars:
                    removed = chars.pop()
                    w = _wcwidth(removed)
                    sys.stdout.write('\b' * w + ' ' * w + '\b' * w)
                    sys.stdout.flush()
                continue

            if byte == 3:               # Ctrl+C
                sys.stdout.write('\n')
                print("退出")
                sys.exit(0)

            if byte == 4:               # Ctrl+D
                sys.stdout.write('\n')
                sys.exit(0)

            if byte == 0x1B:            # ESC / 方向键等转义序列，丢弃
                nxt = os.read(fd, 1)
                if nxt == b'[':
                    while True:
                        nb = os.read(fd, 1)
                        if nb and 0x40 <= nb[0] <= 0x7E:
                            break
                continue

            if byte < 0x20:             # 其他控制字符，忽略
                continue

            # 读取完整 UTF-8 字符
            if   byte < 0x80: extra = 0
            elif byte < 0xE0: extra = 1
            elif byte < 0xF0: extra = 2
            else:              extra = 3
            raw = b0 + (_read_exact(fd, extra) if extra else b'')
            try:
                ch = raw.decode('utf-8')
            except UnicodeDecodeError:
                continue

            chars.append(ch)
            sys.stdout.write(ch)
            sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)

    result = ''.join(chars).strip()
    if result.lower() == 'q':
        print("退出")
        sys.exit(0)
    return result


def select_mode():
    """步骤 1：选择字模类型"""
    print("请选择字模类型:（任意步骤输入 q 退出）")
    print("  1. ASCII（数字 / 符号等）")
    print("  2. 中文")
    while True:
        choice = ask("请输入选项 (1/2): ")
        if choice == '1':
            return 'ascii'
        if choice == '2':
            return 'chinese'
        print("❌ 请输入 1 或 2")


def find_fonts():
    """查找项目根目录及 fonts/ 目录下的所有字体文件"""
    seen = set()
    fonts = []
    search_dirs = [PROJECT_ROOT, PROJECT_ROOT / 'fonts']
    for search_dir in search_dirs:
        for ext in ['*.ttf', '*.otf', '*.TTF', '*.OTF']:
            for f in glob.glob(str(search_dir / ext)):
                key = os.path.normcase(os.path.abspath(f))
                if key not in seen:
                    seen.add(key)
                    fonts.append(f)
    return sorted(fonts)


def select_font(fonts):
    """步骤 2：从列表中选择一个字体文件"""
    print(f"找到 {len(fonts)} 个字体文件，请选择:")
    for i, f in enumerate(fonts, 1):
        print(f"  {i}. {Path(f).name}")
    while True:
        raw = ask("请输入字体编号: ")
        try:
            idx = int(raw)
            if 1 <= idx <= len(fonts):
                return fonts[idx - 1]
            print(f"❌ 请输入 1-{len(fonts)} 之间的数字")
        except ValueError:
            print(f"❌ 请输入 1-{len(fonts)} 之间的数字")


def input_size():
    """步骤 3：输入字体尺寸，支持 12x12 / 12X12 / 12*12"""
    while True:
        raw = ask("字体尺寸 (如 12x12): ")
        normalized = re.sub(r'[xX*]', 'x', raw)
        parts = normalized.split('x')
        if len(parts) == 2:
            try:
                w, h = int(parts[0].strip()), int(parts[1].strip())
                if 1 <= w <= 200 and 1 <= h <= 200:
                    return w, h
                print("❌ 尺寸范围应在 1-200 之间，你应该输入 12x12 这样")
            except ValueError:
                print("❌ 请输入有效数字，你应该输入 12x12 这样")
        else:
            print("❌ 格式错误，你应该输入 12x12 这样")


def input_ascii_chars():
    """ASCII 模式：输入字符集"""
    print(f"默认字符集: ASCII 033~126（! 到 ~，共 {len(ASCII_DEFAULT_CHARS)} 个可打印字符）")
    print("如需自定义，请直接输入想要的字符（如: 0123456789:.）")
    custom = ask("自定义字符集（直接回车使用默认）: ")
    return custom if custom else ASCII_DEFAULT_CHARS


def input_chinese_chars():
    """中文模式：输入要生成的汉字"""
    print("请输入要生成的中文字符（连续输入，无需空格）:")
    print("示例: 星期一二三四五六日温度湿度年月")
    while True:
        chars = ask("中文字符: ")
        if not chars:
            print("❌ 请至少输入一个字符")
            continue
        # 去重保序
        seen, unique = set(), ''
        for c in chars:
            if c not in seen:
                seen.add(c)
                unique += c
        return unique


# ─────────────────────────── 代码生成 ─────────────────────────────

def make_label(font_path, target_w, target_h, mode):
    stem = Path(font_path).stem.replace('-', '_').replace(' ', '_')
    prefix = 'ascii' if mode == 'ascii' else 'chinese'
    return f"Font_{prefix}_{stem}_{target_w}_{target_h}"


def generate_ascii_cpp(font_path, target_w, target_h, chars, output_path, char_data=None):
    """生成 ASCII 字模 C++ 文件（含 Font 结构体定义）"""
    label     = make_label(font_path, target_w, target_h, 'ascii')
    stride    = (target_w + 7) // 8
    font_name = Path(font_path).stem

    lines = [
        f"//",
        f"// 字体: {font_name}",
        f"// 模式: ASCII",
        f"// 尺寸: {target_w}x{target_h}  字符集: {chars}",
        f"// 生成工具: gen_font_digits.py",
        f"//",
        f"",
        f'#include "{Path(output_path).stem}.h"',
        f"#include <cstring>",
        f"",
        f"namespace {{",
        f"",
        f"static const int  k{label}_W      = {target_w};",
        f"static const int  k{label}_H      = {target_h};",
        f"static const int  k{label}_Stride = {stride};",
        # 转义 C 字符串中的反斜杠和双引号
        f'static const char k{label}_Chars[] = "{chars.replace(chr(92), chr(92)*2).replace(chr(34), chr(92)+chr(34))}";',
        f"static const uint8_t k{label}_Glyphs[{len(chars)}][{stride * target_h}] = {{",
    ]

    if char_data is None:
        char_data = []
        for ch in chars:
            if ch == ':':
                data = draw_colon(target_w, target_h)
            elif ch == '.':
                data = draw_dot(target_w, target_h)
            else:
                data = render_char(ch, target_w, target_h, font_path)
            char_data.append((ch, data))
    for ch, data in char_data:
        lines.append("    {" + ','.join(f'0x{b:02X}' for b in data) + f"}}, // '{ch}'")

    lines += [
        "};",
        "",
        f"static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,",
        f"                               int &stride, int &advX, const void*) {{",
        f"    if (cp > 127) return nullptr;",
        f"    const char* pos = strchr(k{label}_Chars, static_cast<char>(cp));",
        f"    if (!pos) return nullptr;",
        f"    w = k{label}_W; h = k{label}_H;",
        f"    stride = k{label}_Stride; advX = k{label}_W;",
        f"    return k{label}_Glyphs[pos - k{label}_Chars];",
        f"}}",
        f"",
        f"}} // namespace",
        f"",
        f"const Font k{label} = {{",
        f"    /* lineHeight */ k{label}_H,",
        f"    /* getGlyph   */ getGlyph,",
        f"    /* data       */ nullptr,",
        f"    /* fallback   */ nullptr,",
        f"}};",
    ]

    output_path.write_text('\n'.join(lines), encoding='utf-8')

    h_lines = [
        f"//",
        f"// 字体: {font_name}  模式: ASCII  尺寸: {target_w}x{target_h}",
        f"//",
        f"",
        f"#pragma once",
        f"",
        f'#include "../Font.h"',
        f"",
        f"extern const Font k{label};",
    ]
    output_path.with_suffix('.h').write_text('\n'.join(h_lines), encoding='utf-8')
    return char_data


def generate_chinese_cpp(font_path, target_w, target_h, chars, output_path, char_data=None):
    """生成中文字模 C++ 文件（含 Font 结构体定义）"""
    label     = make_label(font_path, target_w, target_h, 'chinese')
    stride    = (target_w + 7) // 8
    font_name = Path(font_path).stem

    # 保持用户输入顺序（input_chinese_chars 已去重）
    char_list = list(chars)

    lines = [
        f"//",
        f"// 字体: {font_name}",
        f"// 模式: 中文",
        f"// 尺寸: {target_w}x{target_h}  字符数: {len(char_list)}",
        f"// 生成工具: gen_font_digits.py",
        f"//",
        f"",
        f'#include "{Path(output_path).stem}.h"',
        f"",
        f"namespace {{",
        f"",
        f"static const int k{label}_W      = {target_w};",
        f"static const int k{label}_H      = {target_h};",
        f"static const int k{label}_Stride = {stride};",
        f"",
        f"struct GlyphEntry {{",
        f"    uint32_t codepoint;",
        f"    uint8_t  data[{stride * target_h}];",
        f"}};",
        f"",
        f"// 按用户输入顺序排列（线性查找）",
        f"static const GlyphEntry k{label}_Glyphs[] = {{",
    ]

    if char_data is None:
        char_data = []
        for ch in char_list:
            data = render_char(ch, target_w, target_h, font_path)
            char_data.append((ch, data))
    for ch, data in char_data:
        cp = ord(ch)
        lines.append(
            f"    {{0x{cp:04X}, {{{','.join(f'0x{b:02X}' for b in data)}}}}}, // '{ch}'"
        )

    lines += [
        "};",
        f"static const int k{label}_Count =",
        f"    static_cast<int>(sizeof(k{label}_Glyphs) / sizeof(k{label}_Glyphs[0]));",
        f"",
        f"static const uint8_t* getGlyph(uint32_t cp, int &w, int &h,",
        f"                               int &stride, int &advX, const void*) {{",
        f"    for (int i = 0; i < k{label}_Count; ++i) {{",
        f"        if (k{label}_Glyphs[i].codepoint == cp) {{",
        f"            w = k{label}_W; h = k{label}_H;",
        f"            stride = k{label}_Stride; advX = k{label}_W;",
        f"            return k{label}_Glyphs[i].data;",
        f"        }}",
        f"    }}",
        f"    return nullptr;",
        f"}}",
        f"",
        f"}} // namespace",
        f"",
        f"const Font k{label} = {{",
        f"    /* lineHeight */ k{label}_H,",
        f"    /* getGlyph   */ getGlyph,",
        f"    /* data       */ nullptr,",
        f"    /* fallback   */ nullptr,",
        f"}};",
    ]

    output_path.write_text('\n'.join(lines), encoding='utf-8')

    h_lines = [
        f"//",
        f"// 字体: {font_name}  模式: 中文  尺寸: {target_w}x{target_h}",
        f"//",
        f"",
        f"#pragma once",
        f"",
        f'#include "../Font.h"',
        f"",
        f"extern const Font k{label};",
    ]
    output_path.with_suffix('.h').write_text('\n'.join(h_lines), encoding='utf-8')
    return char_data


# ─────────────────────────── 主流程 ─────────────────────────────

def main():
    print("=" * 60)
    print("字体转换工具 - TTF/OTF 转 C 字模")
    print("=" * 60)
    print()

    # 步骤 1：选择模式
    mode = select_mode()
    print()

    # 步骤 2：选择字体文件
    fonts = find_fonts()
    if not fonts:
        print("❌ 未在项目根目录找到字体文件（ttf/otf）")
        print(f"   项目根目录: {PROJECT_ROOT}")
        return
    print()
    font_path = select_font(fonts)
    print(f"✓ 已选择: {Path(font_path).name}")
    print()

    # 步骤 3：输入尺寸
    print("请输入字模尺寸，支持 12x12 / 12X12 / 12*12：")
    target_w, target_h = input_size()
    print(f"✓ 尺寸: {target_w}x{target_h}")
    print()

    # 字符集
    if mode == 'ascii':
        chars = input_ascii_chars()
    else:
        chars = input_chinese_chars()
    print(f"✓ 字符集: {chars}  ({len(chars)} 个字符)")
    print()

    # 确认
    label           = make_label(font_path, target_w, target_h, mode)
    output_path     = FONTS_OUTPUT_DIR / f"{label}.cpp"
    mode_label      = 'ASCII' if mode == 'ascii' else '中文'
    print("-" * 60)
    print("配置确认:")
    print(f"  模式    : {mode_label}")
    print(f"  字体    : {Path(font_path).name}")
    print(f"  尺寸    : {target_w}x{target_h}")
    print(f"  字符数  : {len(chars)}")
    print(f"  输出    : {output_path}")
    print("-" * 60)
    confirm = ask("确认生成？(y/n): ").lower()
    if confirm not in ('y', 'yes', '是'):
        print("取消操作")
        return
    print()

    # 渲染字模数据
    print("正在渲染字模数据，请稍候...")
    try:
        if mode == 'ascii':
            char_data = []
            for ch in chars:
                if ch == ':':
                    data = draw_colon(target_w, target_h)
                elif ch == '.':
                    data = draw_dot(target_w, target_h)
                else:
                    data = render_char(ch, target_w, target_h, font_path)
                char_data.append((ch, data))
        else:
            char_data = [
                (ch, render_char(ch, target_w, target_h, font_path))
                for ch in chars
            ]
    except Exception as e:
        print(f"❌ 渲染失败: {e}")
        import traceback
        traceback.print_exc()
        return

    # 预览
    print_glyph_preview(char_data, target_w, target_h)

    # 确认是否写入
    print("是否将字模数据写入 fonts 目录？")
    print("  1. 是")
    print("  2. 否")
    while True:
        write_choice = ask("请选择 (1/2): ")
        if write_choice == '1':
            break
        if write_choice == '2':
            print("取消，未写入任何文件")
            return
        print("❌ 请输入 1 或 2")

    # 写入文件
    FONTS_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        if mode == 'ascii':
            generate_ascii_cpp(font_path, target_w, target_h, chars, output_path, char_data)
        else:
            generate_chinese_cpp(font_path, target_w, target_h, chars, output_path, char_data)

        print(f"✓ 写入成功: {output_path.name}")
        print(f"✓ 头文件  : {label}.h")
        print()
        print("在 C++ 中使用示例:")
        print(f'  #include "fonts/{label}.h"')
        print(f"  gui.setFont(&k{label});")
        print(f'  gui.drawText(x, y, "...");')
    except Exception as e:
        print(f"❌ 写入失败: {e}")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()
