#!/usr/bin/env python3
"""
Generate all SPIFFS-backed project font bins.

The binary layout is compatible with FontBinLoader F24BIN1 v1:
64-byte header, sorted Unicode index, then contiguous 1bpp bitmaps.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import subprocess
from typing import Iterable

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FONTS_DIR = PROJECT_ROOT / "fonts"
DATA_DIR = PROJECT_ROOT / "data"

ZH_MAIN_FONT = FONTS_DIR / "AlibabaPuHuiTi-3-65-m-Medium.otf"
ZH_SUB_FONT = FONTS_DIR / "AlibabaPuHuiTi-3-45-Light.otf"
EN_FONT = FONTS_DIR / "NotoSans-Medium.ttf"

MAGIC = b"F24BIN1\0"
VERSION = 1
HEADER_SIZE = 64
INDEX_ENTRY_SIZE = 16
FLAGS_1BPP_MSB_LEFT = 0x0003
HEADER_STRUCT = struct.Struct("<8sHHHHIIIIHHHH24s")
INDEX_STRUCT = struct.Struct("<IIHHHH")

GB2312_LEVEL1_COUNT = 3755
ASCII_SYMBOLS = " !\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`{|}~"
EN_SYMBOLS = (
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
)
CJK_SYMBOLS = "\uff0c\u3002\uff01\uff1f\uff1a\uff1b\uff08\uff09\u3010\u3011\u300a\u300b\u3001\u2103%\u00b1\u00d7\u00f7"
CJK_PUNCT_FALLBACK = set("\uff0c\u3002\uff01\uff1f\uff1a\uff1b\uff08\uff09\u3010\u3011\u300a\u300b\u3001")
DIGIT_CHARS = "0123456789"


@dataclass(frozen=True)
class FontTarget:
    key: str
    output_name: str
    primary_font: Path
    line_height: int
    glyph_chars: str
    mode: str
    expected_count: int
    fallback_font: Path | None = None


def unique_chars(chars: Iterable[str]) -> str:
    return "".join(dict.fromkeys(chars))


def gb2312_level1_chars() -> str:
    chars: list[str] = []
    for hi in range(0xB0, 0xD8):
        for lo in range(0xA1, 0xFF):
            if hi == 0xD7 and lo > 0xF9:
                continue
            chars.append(bytes([hi, lo]).decode("gb2312"))
    if len(chars) != GB2312_LEVEL1_COUNT:
        raise RuntimeError(f"GB2312 level-1 count mismatch: {len(chars)}")
    return "".join(chars)


def to_1bpp(canvas_l: Image.Image, width: int, height: int, threshold: int) -> bytes:
    stride = (width + 7) // 8
    data = bytearray()
    for row in range(height):
        for b_idx in range(stride):
            b = 0
            for bit in range(8):
                x = b_idx * 8 + bit
                if x < width and canvas_l.getpixel((x, row)) > threshold:
                    b |= 1 << (7 - bit)
            data.append(b)
    return bytes(data)


class Renderer:
    def __init__(self) -> None:
        self._font_cache: dict[tuple[Path, int], ImageFont.FreeTypeFont] = {}

    def font(self, path: Path, size: int) -> ImageFont.FreeTypeFont:
        key = (path, size)
        if key not in self._font_cache:
            self._font_cache[key] = ImageFont.truetype(str(path), size)
        return self._font_cache[key]

    def render_variable(self, ch: str, font_path: Path, line_height: int,
                        font_size: int, threshold: int = 140) -> dict:
        font = self.font(font_path, font_size)
        bbox = font.getbbox(ch, anchor="ls")
        advance = max(1, round(font.getlength(ch)))
        glyph_w = max(0, bbox[2] - bbox[0])
        width = max(advance, glyph_w, 1) + 2
        stride = (width + 7) // 8
        baseline = min(line_height - 1, max(1, round(line_height * 0.82)))

        canvas = Image.new("L", (width, line_height), 0)
        draw_x = 1 - bbox[0]
        ImageDraw.Draw(canvas).text((draw_x, baseline), ch, font=font, fill=255, anchor="ls")

        return {
            "codepoint": ord(ch),
            "width": width,
            "height": line_height,
            "stride": stride,
            "advance": width,
            "bitmap": to_1bpp(canvas, width, line_height, threshold),
        }

    def render_square_fit(self, ch: str, font_path: Path, size: int,
                          threshold: int = 70, scale: int = 4) -> dict:
        stride = (size + 7) // 8
        render_pad = 10 * scale
        font = self.font(font_path, size * scale)
        img = Image.new("L", (size * scale + render_pad * 2,
                              size * scale + render_pad * 2), 0)
        ImageDraw.Draw(img).text((render_pad, render_pad), ch, font=font, fill=255)

        bbox = img.getbbox()
        if not bbox:
            bitmap = bytes(stride * size)
        else:
            cropped = img.crop(bbox)
            margin = max(2, round(size / 12))
            avail_w = max(1, size - margin * 2)
            avail_h = max(1, size - margin * 2)
            ratio = min(avail_w / cropped.width, avail_h / cropped.height)
            new_w = max(1, round(cropped.width * ratio))
            new_h = max(1, round(cropped.height * ratio))
            scaled = cropped.resize((new_w, new_h), Image.LANCZOS)
            canvas = Image.new("L", (size, size), 0)
            canvas.paste(scaled, ((size - new_w) // 2, max(0, (size - new_h) // 2)))
            bitmap = to_1bpp(canvas, size, size, threshold)

        return {
            "codepoint": ord(ch),
            "width": size,
            "height": size,
            "stride": stride,
            "advance": size,
            "bitmap": bitmap,
        }

    def render_square_preserved(self, ch: str, font_path: Path, size: int,
                                threshold: int = 70, scale: int = 4) -> dict:
        stride = (size + 7) // 8
        font = self.font(font_path, size * scale)
        canvas = Image.new("L", (size * scale, size * scale), 0)
        ascent, _descent = font.getmetrics()
        baseline = min(size * scale - 1, ascent)
        ImageDraw.Draw(canvas).text((0, baseline), ch, font=font, fill=255, anchor="ls")
        scaled = canvas.resize((size, size), Image.LANCZOS)

        return {
            "codepoint": ord(ch),
            "width": size,
            "height": size,
            "stride": stride,
            "advance": size,
            "bitmap": to_1bpp(scaled, size, size, threshold),
        }

    def render_digit_fit(self, ch: str, font_path: Path, line_height: int,
                         threshold: int = 140, scale: int = 4) -> dict:
        # 首页数字需要接近 line_height 的视觉高度；直接用同字号渲染时，
        # NotoSans 的实际可见高度只有约 0.74em，看起来会误判为字号偏小。
        font = self.font(font_path, line_height * scale)
        pad = line_height * scale
        canvas_hi = Image.new("L", (line_height * scale * 3,
                                    line_height * scale * 3), 0)
        ascent, _descent = font.getmetrics()
        ImageDraw.Draw(canvas_hi).text((pad, pad + ascent), ch,
                                       font=font, fill=255, anchor="ls")

        bbox = canvas_hi.getbbox()
        cell_w = max(1, round(line_height * 0.68))
        if not bbox:
            width = cell_w
            bitmap = bytes(((width + 7) // 8) * line_height)
            return {
                "codepoint": ord(ch),
                "width": width,
                "height": line_height,
                "stride": (width + 7) // 8,
                "advance": width,
                "bitmap": bitmap,
            }

        cropped = canvas_hi.crop(bbox)
        target_h = max(1, line_height - 4)
        ratio = target_h / cropped.height
        new_w = max(1, round(cropped.width * ratio))
        new_h = target_h
        width = max(cell_w, new_w + 4)
        stride = (width + 7) // 8

        scaled = cropped.resize((new_w, new_h), Image.LANCZOS)
        canvas = Image.new("L", (width, line_height), 0)
        canvas.paste(scaled, ((width - new_w) // 2, (line_height - new_h) // 2))
        return {
            "codepoint": ord(ch),
            "width": width,
            "height": line_height,
            "stride": stride,
            "advance": width,
            "bitmap": to_1bpp(canvas, width, line_height, threshold),
        }


def chinese_glyphs(target: FontTarget, renderer: Renderer) -> list[dict]:
    gb_chars = set(gb2312_level1_chars())
    glyphs: list[dict] = []
    for ch in target.glyph_chars:
        if ch in gb_chars:
            glyphs.append(renderer.render_square_fit(ch, target.primary_font, target.line_height))
        elif ord(ch) < 128:
            glyphs.append(renderer.render_variable(ch, target.primary_font, target.line_height,
                                                  target.line_height))
        else:
            glyphs.append(renderer.render_square_preserved(ch, target.primary_font,
                                                          target.line_height))
    return glyphs


def english_glyphs(target: FontTarget, renderer: Renderer) -> list[dict]:
    glyphs: list[dict] = []
    for ch in target.glyph_chars:
        if ch in CJK_PUNCT_FALLBACK:
            glyphs.append(renderer.render_square_preserved(ch, target.fallback_font or target.primary_font,
                                                          target.line_height))
        else:
            glyphs.append(renderer.render_variable(ch, target.primary_font, target.line_height,
                                                  target.line_height))
    return glyphs


def target_glyphs(target: FontTarget, renderer: Renderer) -> list[dict]:
    if target.mode == "zh":
        glyphs = chinese_glyphs(target, renderer)
    elif target.mode == "en":
        glyphs = english_glyphs(target, renderer)
    elif target.mode == "digits":
        glyphs = [
            renderer.render_variable(ch, target.primary_font, target.line_height, target.line_height)
            for ch in target.glyph_chars
        ]
    elif target.mode == "digits_fit":
        glyphs = [
            renderer.render_digit_fit(ch, target.primary_font, target.line_height)
            for ch in target.glyph_chars
        ]
    else:
        raise ValueError(f"unknown target mode: {target.mode}")

    by_cp = {glyph["codepoint"]: glyph for glyph in glyphs}
    if len(by_cp) != target.expected_count:
        raise RuntimeError(
            f"{target.key}: glyph count mismatch, expected {target.expected_count}, got {len(by_cp)}"
        )
    return sorted(by_cp.values(), key=lambda item: item["codepoint"])


def write_font_bin(target: FontTarget, renderer: Renderer) -> dict:
    glyphs = target_glyphs(target, renderer)
    max_glyph_bytes = max(len(glyph["bitmap"]) for glyph in glyphs)
    index_offset = HEADER_SIZE
    bitmap_offset = HEADER_SIZE + len(glyphs) * INDEX_ENTRY_SIZE
    index_data = bytearray()
    bitmap_data = bytearray()

    for glyph in glyphs:
        entry_offset = len(bitmap_data)
        index_data.extend(INDEX_STRUCT.pack(
            glyph["codepoint"],
            entry_offset,
            glyph["width"],
            glyph["height"],
            glyph["stride"],
            glyph["advance"],
        ))
        bitmap_data.extend(glyph["bitmap"])

    file_size = HEADER_SIZE + len(index_data) + len(bitmap_data)
    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION,
        HEADER_SIZE,
        INDEX_ENTRY_SIZE,
        FLAGS_1BPP_MSB_LEFT,
        len(glyphs),
        index_offset,
        bitmap_offset,
        file_size,
        target.line_height,
        target.line_height,
        max_glyph_bytes,
        0,
        bytes(24),
    )

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    output_path = DATA_DIR / target.output_name
    output_path.write_bytes(header + index_data + bitmap_data)
    verify_output(output_path, target, glyphs, file_size)
    return {
        "path": output_path,
        "glyph_count": len(glyphs),
        "max_glyph_bytes": max_glyph_bytes,
        "file_size": file_size,
    }


def verify_output(path: Path, target: FontTarget, glyphs: list[dict], file_size: int) -> None:
    data = path.read_bytes()
    if len(data) != file_size:
        raise RuntimeError(f"{target.key}: written size mismatch")
    header = HEADER_STRUCT.unpack_from(data, 0)
    if header[0] != MAGIC or header[1] != VERSION:
        raise RuntimeError(f"{target.key}: bad header after write")
    by_cp = {glyph["codepoint"] for glyph in glyphs}
    for ch in target.glyph_chars:
        if ord(ch) not in by_cp:
            raise RuntimeError(f"{target.key}: missing glyph U+{ord(ch):04X}")


def make_targets() -> list[FontTarget]:
    zh_chars = unique_chars(gb2312_level1_chars() + ASCII_SYMBOLS + CJK_SYMBOLS)
    en_chars = unique_chars(EN_SYMBOLS + CJK_SYMBOLS)
    return [
        FontTarget("zh", "font_zh_main_24.bin", ZH_MAIN_FONT, 24, zh_chars, "zh", 3815),
        FontTarget("zh", "font_zh_sub_20.bin", ZH_SUB_FONT, 20, zh_chars, "zh", 3815),
        FontTarget("en", "font_en_main_18.bin", EN_FONT, 18, en_chars, "en", 112, ZH_MAIN_FONT),
        FontTarget("en", "font_en_sub_16.bin", EN_FONT, 16, en_chars, "en", 112, ZH_MAIN_FONT),
        FontTarget("digits", "font_digits_60.bin", EN_FONT, 60, DIGIT_CHARS, "digits_fit", 10),
    ]


def clean_old_font_bins(targets: list[FontTarget], all_targets: list[FontTarget]) -> None:
    output_names = {target.output_name for target in targets}
    legacy_names: set[str] = set()
    if len(targets) == len(all_targets):
        legacy_names.add("font24.bin")
        legacy_names.update(path.name for path in DATA_DIR.glob("FONT_*.bin"))
    if any(target.key == "digits" for target in targets):
        legacy_names.add("font_digits_100.bin")
        legacy_names.add("font_digits_40.bin")
        legacy_names.add("font_digits_56.bin")
        legacy_names.add("font_digits_72.bin")

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    for name in sorted(output_names | legacy_names):
        path = DATA_DIR / name
        if path.exists():
            path.unlink()


def ensure_fonts_exist(targets: list[FontTarget]) -> None:
    required = {target.primary_font for target in targets}
    required.update(target.fallback_font for target in targets if target.fallback_font is not None)
    for font_path in sorted(required):
        if not font_path.exists():
            raise FileNotFoundError(f"missing source font: {font_path}")


def selected_targets(all_targets: list[FontTarget], key: str) -> list[FontTarget]:
    if key == "all":
        return all_targets
    return [target for target in all_targets if target.key == key]


def upload_spiffs(pio_env: str) -> None:
    cmd = ["pio", "run", "-e", pio_env, "-t", "uploadfs"]
    print(f"uploadfs: {' '.join(cmd)}")
    subprocess.run(cmd, cwd=PROJECT_ROOT, check=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate project SPIFFS font bins.")
    parser.add_argument("--target", choices=("all", "zh", "en", "digits"), default="all")
    parser.add_argument("--pio-env", default="esp32s3box")
    parser.add_argument("--uploadfs", action="store_true")
    parser.add_argument("--no-clean", action="store_true",
                        help="Do not delete existing data/*.bin before generation.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    all_targets = make_targets()
    targets = selected_targets(all_targets, args.target)
    ensure_fonts_exist(targets)
    if not args.no_clean:
        clean_old_font_bins(targets, all_targets)

    renderer = Renderer()
    for target in targets:
        meta = write_font_bin(target, renderer)
        print(
            f"{target.output_name}: glyphs={meta['glyph_count']} "
            f"maxGlyphBytes={meta['max_glyph_bytes']} fileSize={meta['file_size']}"
        )

    if args.uploadfs:
        upload_spiffs(args.pio_env)


if __name__ == "__main__":
    main()
