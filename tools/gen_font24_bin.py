#!/usr/bin/env python3
"""
Generate data/font24.bin for the SPIFFS-backed 24px UI font.

The file format is intentionally simple for ESP32 runtime parsing:
64-byte header, fixed-size sorted glyph index, then contiguous 1bpp bitmaps.
GB2312 is used only to enumerate the first-level common Chinese characters;
runtime lookup uses the Unicode codepoint decoded by Gui::drawText().
"""

from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).parent.parent
DEFAULT_FONT_PATH = PROJECT_ROOT / "fonts" / "AlibabaPuHuiTi-3-75-SemiBold.ttf"
DEFAULT_OUTPUT_PATH = PROJECT_ROOT / "data" / "font24.bin"

MAGIC = b"F24BIN1\0"
VERSION = 1
HEADER_SIZE = 64
INDEX_ENTRY_SIZE = 16
FLAGS_1BPP_MSB_LEFT = 0x0003
LINE_HEIGHT = 24
DEFAULT_ADVANCE = 24
MAX_GLYPH_BYTES = 72

HEADER_STRUCT = struct.Struct("<8sHHHHIIIIHHHH24s")
INDEX_STRUCT = struct.Struct("<IIHHHH")

ASCII_CHARS = "".join(chr(i) for i in range(32, 127))


def to_1bpp(canvas_l, target_w, target_h, threshold):
    """Convert an L-mode bitmap into row-major, MSB-left 1bpp bytes."""
    stride = (target_w + 7) // 8
    data = bytearray()
    for row in range(target_h):
        for b_idx in range(stride):
            b = 0
            for bit in range(8):
                x = b_idx * 8 + bit
                if x < target_w and canvas_l.getpixel((x, row)) > threshold:
                    b |= 1 << (7 - bit)
            data.append(b)
    return bytes(data)


def gb2312_level1_chars():
    """Enumerate GB2312 first-level Chinese characters: B0A1-D7F9."""
    chars = []
    for hi in range(0xB0, 0xD8):
        for lo in range(0xA1, 0xFF):
            if hi == 0xD7 and lo > 0xF9:
                continue
            chars.append(bytes([hi, lo]).decode("gb2312"))
    if len(chars) != 3755:
        raise RuntimeError(f"GB2312 level-1 count mismatch: {len(chars)}")
    return chars


def render_chinese_char(ch, font_path):
    """
    Render one Chinese glyph into a 24x24 fixed cell.

    This mirrors the existing C-array generator's high-level policy:
    supersample, crop visible bounds, scale into a padded target cell, then
    threshold to the GUI's 1bpp format.
    """
    target_w = 24
    target_h = 24
    scale = 4
    pad = 10 * scale
    font = ImageFont.truetype(str(font_path), target_h * scale)

    img = Image.new("L", (target_w * scale + pad * 2, target_h * scale + pad * 2), 0)
    ImageDraw.Draw(img).text((pad, pad), ch, font=font, fill=255)

    bbox = img.getbbox()
    if not bbox:
        return {
            "codepoint": ord(ch),
            "width": target_w,
            "height": target_h,
            "stride": 3,
            "advance": target_w,
            "bitmap": bytes(3 * target_h),
        }

    cropped = img.crop(bbox)
    glyph_w, glyph_h = cropped.size
    avail_w = target_w - 4
    avail_h = target_h - 4
    ratio = min(avail_w / glyph_w, avail_h / glyph_h)
    new_w = max(1, round(glyph_w * ratio))
    new_h = max(1, round(glyph_h * ratio))
    scaled = cropped.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new("L", (target_w, target_h), 0)
    canvas.paste(scaled, ((target_w - new_w) // 2, max(0, (target_h - new_h) // 2)))

    return {
        "codepoint": ord(ch),
        "width": target_w,
        "height": target_h,
        "stride": 3,
        "advance": target_w,
        "bitmap": to_1bpp(canvas, target_w, target_h, threshold=70),
    }


def render_ascii_chars(font_path):
    """
    Render ASCII with a shared baseline and variable width.

    Width and advance are stored per glyph so ASCII text does not inherit the
    24px Chinese grid spacing.
    """
    font = ImageFont.truetype(str(font_path), 21)
    rendered = []
    for ch in ASCII_CHARS:
        bbox = font.getbbox(ch, anchor="ls")
        advance = max(1, round(font.getlength(ch)))
        glyph_w = max(1, bbox[2] - bbox[0])
        width = max(advance, glyph_w) + 2
        height = LINE_HEIGHT
        stride = (width + 7) // 8
        baseline = height - 6

        canvas = Image.new("L", (width, height), 0)
        draw_x = 1 - bbox[0]
        ImageDraw.Draw(canvas).text((draw_x, baseline), ch, font=font, fill=255, anchor="ls")

        bitmap = to_1bpp(canvas, width, height, threshold=140)
        if len(bitmap) > MAX_GLYPH_BYTES:
            raise RuntimeError(f"ASCII glyph too large: {ch!r} -> {len(bitmap)} bytes")

        rendered.append({
            "codepoint": ord(ch),
            "width": width,
            "height": height,
            "stride": stride,
            "advance": width,
            "bitmap": bitmap,
        })
    return rendered


def build_font_bin(font_path, output_path):
    glyphs = render_ascii_chars(font_path)
    glyphs.extend(render_chinese_char(ch, font_path) for ch in gb2312_level1_chars())
    glyphs.sort(key=lambda item: item["codepoint"])

    if len(glyphs) != 3850:
        raise RuntimeError(f"glyph count mismatch: {len(glyphs)}")

    index_offset = HEADER_SIZE
    bitmap_offset = HEADER_SIZE + len(glyphs) * INDEX_ENTRY_SIZE
    bitmap_data = bytearray()
    index_data = bytearray()

    for glyph in glyphs:
        entry_offset = len(bitmap_data)
        bitmap = glyph["bitmap"]
        index_data.extend(INDEX_STRUCT.pack(
            glyph["codepoint"],
            entry_offset,
            glyph["width"],
            glyph["height"],
            glyph["stride"],
            glyph["advance"],
        ))
        bitmap_data.extend(bitmap)

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
        LINE_HEIGHT,
        DEFAULT_ADVANCE,
        MAX_GLYPH_BYTES,
        0,
        bytes(24),
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(header + index_data + bitmap_data)
    return {
        "glyph_count": len(glyphs),
        "index_offset": index_offset,
        "bitmap_offset": bitmap_offset,
        "file_size": file_size,
        "glyphs": glyphs,
    }


def verify_output(output_path, meta):
    data = output_path.read_bytes()
    if len(data) != meta["file_size"]:
        raise RuntimeError("written file size mismatch")
    header = HEADER_STRUCT.unpack_from(data, 0)
    if header[0] != MAGIC or header[1] != VERSION:
        raise RuntimeError("header magic/version verification failed")
    by_cp = {glyph["codepoint"]: glyph for glyph in meta["glyphs"]}
    for ch in ("A", "0", "啊", "中", "座"):
        if ord(ch) not in by_cp:
            raise RuntimeError(f"missing verification glyph: {ch}")


def main():
    meta = build_font_bin(DEFAULT_FONT_PATH, DEFAULT_OUTPUT_PATH)
    verify_output(DEFAULT_OUTPUT_PATH, meta)
    print(f"font: {DEFAULT_FONT_PATH}")
    print(f"output: {DEFAULT_OUTPUT_PATH}")
    print(f"glyph_count: {meta['glyph_count']}")
    print(f"gb2312_level1: 3755")
    print(f"indexOffset: {meta['index_offset']}")
    print(f"bitmapOffset: {meta['bitmap_offset']}")
    print(f"fileSize: {meta['file_size']}")


if __name__ == "__main__":
    main()
