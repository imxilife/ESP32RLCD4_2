#!/usr/bin/env python3
from pathlib import Path
import math
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
RES_IMG = ROOT / "res" / "img"
RES_ICONS = ROOT / "res" / "icons"
OUT_DIR = ROOT / "lib" / "ui" / "assets"
FONT_DIR = ROOT / "lib" / "ui" / "gui" / "fonts"

STATUS_SIZE = 16
WEATHER_SIZE = 20
HOME_ROW_ICON_SIZE = 20
FONT_SIZE = 20
FONT_CHARS = "今日无事，很开心"


def pack_1bpp(img, threshold=96):
    img = img.convert("L")
    w, h = img.size
    stride = (w + 7) // 8
    data = []
    for y in range(h):
        for bx in range(stride):
            b = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and img.getpixel((x, y)) > threshold:
                    b |= 1 << (7 - bit)
            data.append(b)
    return data


def png_to_1bpp(path, size):
    rgba = Image.open(path).convert("RGBA").resize((size, size), Image.LANCZOS)
    out = Image.new("L", (size, size), 0)
    pix = rgba.load()
    for y in range(size):
        for x in range(size):
            r, g, b, a = pix[x, y]
            lum = (r + g + b) // 3
            if a > 32 and lum < 245:
                out.putpixel((x, y), 255)
    return pack_1bpp(out, 96)


def tokens(path_d):
    return re.findall(r"[AaCcHhLlMmQqSsTtVvZz]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?", path_d)


def is_cmd(token):
    return len(token) == 1 and token.isalpha()


def cubic(p0, p1, p2, p3, steps=18):
    pts = []
    for i in range(1, steps + 1):
        t = i / steps
        mt = 1 - t
        x = mt**3 * p0[0] + 3 * mt**2 * t * p1[0] + 3 * mt * t**2 * p2[0] + t**3 * p3[0]
        y = mt**3 * p0[1] + 3 * mt**2 * t * p1[1] + 3 * mt * t**2 * p2[1] + t**3 * p3[1]
        pts.append((x, y))
    return pts


def quadratic(p0, p1, p2, steps=16):
    pts = []
    for i in range(1, steps + 1):
        t = i / steps
        mt = 1 - t
        x = mt * mt * p0[0] + 2 * mt * t * p1[0] + t * t * p2[0]
        y = mt * mt * p0[1] + 2 * mt * t * p1[1] + t * t * p2[1]
        pts.append((x, y))
    return pts


def arc(p0, rx, ry, angle_deg, large_arc, sweep, p1, steps=20):
    x1, y1 = p0
    x2, y2 = p1
    rx = abs(rx)
    ry = abs(ry)
    if rx == 0 or ry == 0:
        return [p1]

    phi = math.radians(angle_deg % 360)
    cos_phi = math.cos(phi)
    sin_phi = math.sin(phi)
    dx = (x1 - x2) / 2
    dy = (y1 - y2) / 2
    x1p = cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy

    lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if lam > 1:
        scale = math.sqrt(lam)
        rx *= scale
        ry *= scale

    sign = -1 if large_arc == sweep else 1
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    coef = 0 if den == 0 else sign * math.sqrt(max(0, num / den))
    cxp = coef * (rx * y1p / ry)
    cyp = coef * (-ry * x1p / rx)
    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2

    def vangle(ux, uy, vx, vy):
        dot = ux * vx + uy * vy
        det = ux * vy - uy * vx
        return math.atan2(det, dot)

    ux = (x1p - cxp) / rx
    uy = (y1p - cyp) / ry
    vx = (-x1p - cxp) / rx
    vy = (-y1p - cyp) / ry
    start = vangle(1, 0, ux, uy)
    delta = vangle(ux, uy, vx, vy)
    if not sweep and delta > 0:
        delta -= 2 * math.pi
    elif sweep and delta < 0:
        delta += 2 * math.pi

    pts = []
    n = max(4, int(abs(delta) / (math.pi / steps)) + 1)
    for i in range(1, n + 1):
        theta = start + delta * i / n
        x = cx + rx * math.cos(theta) * cos_phi - ry * math.sin(theta) * sin_phi
        y = cy + rx * math.cos(theta) * sin_phi + ry * math.sin(theta) * cos_phi
        pts.append((x, y))
    return pts


def parse_path(path_d):
    ts = tokens(path_d)
    i = 0
    cmd = None
    cur = (0.0, 0.0)
    start = (0.0, 0.0)
    last_c = None
    last_q = None
    contours = []
    contour = []

    def num():
        nonlocal i
        v = float(ts[i])
        i += 1
        return v

    def add(p):
        nonlocal cur
        contour.append(p)
        cur = p

    def finish():
        nonlocal contour
        if len(contour) >= 3:
            contours.append(contour)
        contour = []

    while i < len(ts):
        if is_cmd(ts[i]):
            cmd = ts[i]
            i += 1
        if cmd is None:
            break

        rel = cmd.islower()
        c = cmd.upper()
        last_c = last_c if c in ("C", "S") else None
        last_q = last_q if c in ("Q", "T") else None

        if c == "M":
            first = True
            while i < len(ts) and not is_cmd(ts[i]):
                x, y = num(), num()
                if rel:
                    x += cur[0]
                    y += cur[1]
                if first:
                    finish()
                    cur = (x, y)
                    start = cur
                    contour = [cur]
                    first = False
                else:
                    add((x, y))
            cmd = "l" if rel else "L"
        elif c == "L":
            while i < len(ts) and not is_cmd(ts[i]):
                x, y = num(), num()
                if rel:
                    x += cur[0]
                    y += cur[1]
                add((x, y))
        elif c == "H":
            while i < len(ts) and not is_cmd(ts[i]):
                x = num()
                if rel:
                    x += cur[0]
                add((x, cur[1]))
        elif c == "V":
            while i < len(ts) and not is_cmd(ts[i]):
                y = num()
                if rel:
                    y += cur[1]
                add((cur[0], y))
        elif c == "C":
            while i < len(ts) and not is_cmd(ts[i]):
                p1 = (num(), num())
                p2 = (num(), num())
                p3 = (num(), num())
                if rel:
                    p1 = (p1[0] + cur[0], p1[1] + cur[1])
                    p2 = (p2[0] + cur[0], p2[1] + cur[1])
                    p3 = (p3[0] + cur[0], p3[1] + cur[1])
                for p in cubic(cur, p1, p2, p3):
                    add(p)
                last_c = p2
        elif c == "S":
            while i < len(ts) and not is_cmd(ts[i]):
                p1 = (2 * cur[0] - last_c[0], 2 * cur[1] - last_c[1]) if last_c else cur
                p2 = (num(), num())
                p3 = (num(), num())
                if rel:
                    p2 = (p2[0] + cur[0], p2[1] + cur[1])
                    p3 = (p3[0] + cur[0], p3[1] + cur[1])
                for p in cubic(cur, p1, p2, p3):
                    add(p)
                last_c = p2
        elif c == "Q":
            while i < len(ts) and not is_cmd(ts[i]):
                p1 = (num(), num())
                p2 = (num(), num())
                if rel:
                    p1 = (p1[0] + cur[0], p1[1] + cur[1])
                    p2 = (p2[0] + cur[0], p2[1] + cur[1])
                for p in quadratic(cur, p1, p2):
                    add(p)
                last_q = p1
        elif c == "T":
            while i < len(ts) and not is_cmd(ts[i]):
                p1 = (2 * cur[0] - last_q[0], 2 * cur[1] - last_q[1]) if last_q else cur
                p2 = (num(), num())
                if rel:
                    p2 = (p2[0] + cur[0], p2[1] + cur[1])
                for p in quadratic(cur, p1, p2):
                    add(p)
                last_q = p1
        elif c == "A":
            while i < len(ts) and not is_cmd(ts[i]):
                rx, ry, rot = num(), num(), num()
                large, sw = int(num()), int(num())
                p = (num(), num())
                if rel:
                    p = (p[0] + cur[0], p[1] + cur[1])
                for point in arc(cur, rx, ry, rot, large, sw, p):
                    add(point)
        elif c == "Z":
            add(start)
            finish()
            cur = start
        else:
            break

    finish()
    return contours


def point_in_poly(x, y, poly):
    inside = False
    n = len(poly)
    if n < 3:
        return False
    x1, y1 = poly[-1]
    for x2, y2 in poly:
        if ((y1 > y) != (y2 > y)):
            at_x = (x2 - x1) * (y - y1) / (y2 - y1 + 1e-9) + x1
            if x < at_x:
                inside = not inside
        x1, y1 = x2, y2
    return inside


def svg_to_1bpp(path, size):
    text = path.read_text(encoding="utf-8")
    vb = re.search(r'viewBox="([^"]+)"', text)
    if vb:
        minx, miny, vw, vh = [float(x) for x in re.split(r"[ ,]+", vb.group(1).strip())]
    else:
        minx, miny, vw, vh = 0.0, 0.0, 16.0, 16.0

    contours = []
    for d in re.findall(r'<path[^>]*\sd="([^"]+)"', text):
        contours.extend(parse_path(d))

    scale = 4
    big = size * scale
    img = Image.new("L", (big, big), 0)
    for py in range(big):
        y = miny + (py + 0.5) * vh / big
        for px in range(big):
            x = minx + (px + 0.5) * vw / big
            inside = False
            for contour in contours:
                if point_in_poly(x, y, contour):
                    inside = not inside
            if inside:
                img.putpixel((px, py), 255)
    img = img.resize((size, size), Image.LANCZOS)
    return pack_1bpp(img, 64)


def glyph_to_1bpp(ch, size):
    font_path = ROOT / "fonts" / "AlibabaPuHuiTi-3-75-SemiBold.ttf"
    scale = 4
    font = ImageFont.truetype(str(font_path), size * scale)
    pad = size * scale
    img = Image.new("L", (size * scale + pad * 2, size * scale + pad * 2), 0)
    ImageDraw.Draw(img).text((pad, pad), ch, font=font, fill=255)
    bbox = img.getbbox()
    if not bbox:
        return [0] * (((size + 7) // 8) * size)
    cropped = img.crop(bbox)
    ratio = min((size - 2) / cropped.width, (size - 2) / cropped.height)
    resized = cropped.resize((max(1, round(cropped.width * ratio)), max(1, round(cropped.height * ratio))), Image.LANCZOS)
    out = Image.new("L", (size, size), 0)
    out.paste(resized, ((size - resized.width) // 2, (size - resized.height) // 2))
    return pack_1bpp(out, 70)


def fmt_bytes(data):
    return ",".join(f"0x{b:02X}" for b in data)


def write_home_assets():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    status_sources = {
        "Wifi": RES_IMG / "wifi.png",
        "NoWifi": RES_IMG / "no_wifi.png",
        "Battery25": RES_IMG / "battery_25_percent.png",
        "Battery50": RES_IMG / "battery_50_percent.png",
        "Battery75": RES_IMG / "battery_75_percent.png",
        "BatteryFull": RES_IMG / "full_battery.png",
    }
    home_row_sources = {
        "WeatherPlaceholder": RES_IMG / "weather.png",
        "PomodoroClock": RES_IMG / "pomodoro_clock.png",
        "Todo": RES_IMG / "todo.png",
    }

    h = OUT_DIR / "HomeBitmapAssets.h"
    cpp = OUT_DIR / "HomeBitmapAssets.cpp"
    h.write_text(
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        "namespace HomeBitmapAssets {\n"
        f"constexpr int kStatusIconW = {STATUS_SIZE};\n"
        f"constexpr int kStatusIconH = {STATUS_SIZE};\n"
        f"constexpr int kHomeRowIconW = {HOME_ROW_ICON_SIZE};\n"
        f"constexpr int kHomeRowIconH = {HOME_ROW_ICON_SIZE};\n"
        f"constexpr int kWeatherIconW = {WEATHER_SIZE};\n"
        f"constexpr int kWeatherIconH = {WEATHER_SIZE};\n"
        "extern const uint8_t kWifi[];\n"
        "extern const uint8_t kNoWifi[];\n"
        "extern const uint8_t kBattery25[];\n"
        "extern const uint8_t kBattery50[];\n"
        "extern const uint8_t kBattery75[];\n"
        "extern const uint8_t kBatteryFull[];\n"
        "extern const uint8_t kWeatherPlaceholder[];\n"
        "extern const uint8_t kPomodoroClock[];\n"
        "extern const uint8_t kTodo[];\n"
        "const uint8_t* weatherIcon(uint16_t code);\n"
        "}\n",
        encoding="utf-8",
    )

    lines = ['#include "HomeBitmapAssets.h"\n', "namespace HomeBitmapAssets {\n"]
    for name, src in status_sources.items():
        data = png_to_1bpp(src, STATUS_SIZE)
        lines.append(f"const uint8_t k{name}[] = {{{fmt_bytes(data)}}};\n")
    for name, src in home_row_sources.items():
        data = png_to_1bpp(src, HOME_ROW_ICON_SIZE)
        lines.append(f"const uint8_t k{name}[] = {{{fmt_bytes(data)}}};\n")

    weather_entries = []
    for svg in sorted(RES_ICONS.glob("*.svg")):
        stem = svg.stem
        if not stem.isdigit():
            continue
        code = int(stem)
        data = svg_to_1bpp(svg, WEATHER_SIZE)
        arr = f"kWeather{code}"
        lines.append(f"static const uint8_t {arr}[] = {{{fmt_bytes(data)}}};\n")
        weather_entries.append((code, arr))

    lines.append("struct WeatherEntry { uint16_t code; const uint8_t* data; };\n")
    lines.append("static const WeatherEntry kWeatherTable[] = {\n")
    for code, arr in weather_entries:
        lines.append(f"    {{{code}, {arr}}},\n")
    lines.append("};\n")
    lines.append(
        "const uint8_t* weatherIcon(uint16_t code) {\n"
        "    for (const auto& item : kWeatherTable) {\n"
        "        if (item.code == code) return item.data;\n"
        "    }\n"
        "    for (const auto& item : kWeatherTable) {\n"
        "        if (item.code == 999) return item.data;\n"
        "    }\n"
        "    return nullptr;\n"
        "}\n"
    )
    lines.append("}\n")
    cpp.write_text("".join(lines), encoding="utf-8")


def write_home_chinese_font():
    h = FONT_DIR / "Font_chinese_HomeMemo_20_20.h"
    cpp = FONT_DIR / "Font_chinese_HomeMemo_20_20.cpp"
    h.write_text(
        "#pragma once\n\n"
        "#include \"../Font.h\"\n\n"
        "extern const Font kFont_chinese_HomeMemo_20_20;\n",
        encoding="utf-8",
    )

    lines = [
        '#include "Font_chinese_HomeMemo_20_20.h"\n',
        '#include "Font_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20.h"\n\n',
        "namespace {\n",
        f"constexpr int kGlyphW = {FONT_SIZE};\n",
        f"constexpr int kGlyphH = {FONT_SIZE};\n",
        f"constexpr int kStride = {(FONT_SIZE + 7) // 8};\n",
        "struct GlyphEntry { uint32_t codepoint; uint8_t data[kStride * kGlyphH]; };\n",
        "static const GlyphEntry kGlyphs[] = {\n",
    ]
    for ch in dict.fromkeys(FONT_CHARS):
        lines.append(f"    {{0x{ord(ch):04X}, {{{fmt_bytes(glyph_to_1bpp(ch, FONT_SIZE))}}}}},\n")
    lines.extend([
        "};\n",
        "const uint8_t* getGlyph(uint32_t cp, int& w, int& h, int& stride, int& advX, const void*) {\n",
        "    for (const auto& item : kGlyphs) {\n",
        "        if (item.codepoint == cp) { w = kGlyphW; h = kGlyphH; stride = kStride; advX = kGlyphW; return item.data; }\n",
        "    }\n",
        "    return nullptr;\n",
        "}\n",
        "}\n\n",
        "const Font kFont_chinese_HomeMemo_20_20 = {\n",
        "    kGlyphH,\n",
        "    getGlyph,\n",
        "    nullptr,\n",
        "    &kFont_ascii_AlibabaPuHuiTi_3_75_SemiBold_20_20,\n",
        "};\n",
    ])
    cpp.write_text("".join(lines), encoding="utf-8")


if __name__ == "__main__":
    write_home_assets()
    write_home_chinese_font()
    print("generated home assets")
