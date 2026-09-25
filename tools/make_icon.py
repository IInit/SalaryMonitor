#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成插件图标 SalaryMonitor/res/salary_monitor.ico。

仓库不存放二进制文件，图标由本脚本确定性重绘：
    墨绿到深绿的圆角方块 + 金色人民币符号 ¥，表达"进账 / 工资监测"。

用法：
    python tools/make_icon.py                     # 写入 SalaryMonitor/res/salary_monitor.ico
    python tools/make_icon.py --preview out.png   # 额外导出一张 256px PNG 便于查看

仅依赖 Python 标准库（zlib / struct），Windows 与 Linux 均可运行。
"""

import argparse
import os
import struct
import sys
import zlib

# ---- 跨平台输出保护 ----------------------------------------------------------
# Windows 控制台/管道可能是 cp1252 / GBK 等窄编码，print 非 ASCII 内容会抛
# UnicodeEncodeError（GitHub Actions 的 Windows runner 上必现）。统一切到
# UTF-8，并对无法编码的字符降级为转义。
for _stream in ("stdout", "stderr"):
    _s = getattr(sys, _stream, None)
    if _s is not None and hasattr(_s, "reconfigure"):
        try:
            _s.reconfigure(encoding="utf-8", errors="backslashreplace")
        except Exception:                              # noqa: BLE001 - 尽力而为
            pass

# ----------------------------------------------------------------- 图形定义
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
SS = 4                      # 超采样倍数（先放大绘制再缩小，得到抗锯齿边缘）

# 圆角方块渐变（自上而下）：墨绿 -> 深绿
BG_TOP = (0x13, 0x6F, 0x5B)
BG_BOTTOM = (0x05, 0x2C, 0x25)
GLYPH = (0xFF, 0xC8, 0x3A)         # 金色 ¥
GLYPH_EDGE = (0xFF, 0xE3, 0x9B)    # 笔画边缘提亮

RADIUS_RATIO = 0.235        # 圆角半径 / 边长

# ¥ 由 5 个多边形拼成（归一化坐标，原点左上）：两条斜笔画、竖笔画、两条横杠
YEN_POLYS = [
    [(0.290, 0.150), (0.372, 0.150), (0.546, 0.468), (0.464, 0.468)],   # 左斜
    [(0.628, 0.150), (0.710, 0.150), (0.536, 0.468), (0.454, 0.468)],   # 右斜
    [(0.457, 0.440), (0.543, 0.440), (0.543, 0.885), (0.457, 0.885)],   # 竖
    [(0.262, 0.535), (0.738, 0.535), (0.738, 0.607), (0.262, 0.607)],   # 上横
    [(0.262, 0.648), (0.738, 0.648), (0.738, 0.720), (0.262, 0.720)],   # 下横
]


def _blend(dst, src, alpha):
    return tuple(int(round(d + (s - d) * alpha)) for d, s in zip(dst, src))


def _inside_rounded_rect(x, y, size, radius):
    """点是否落在圆角矩形内（坐标已归一化到 [0,size)）。"""
    if x < 0 or y < 0 or x >= size or y >= size:
        return False
    cx = min(max(x, radius), size - radius)
    cy = min(max(y, radius), size - radius)
    dx = x - cx
    dy = y - cy
    if dx == 0.0 and dy == 0.0:
        return True
    return dx * dx + dy * dy <= radius * radius


def _inside_polygon(x, y, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y):
            x_cross = (xj - xi) * (y - yi) / (yj - yi) + xi
            if x < x_cross:
                inside = not inside
        j = i
    return inside


def _on_glyph(x, y, polys):
    for p in polys:
        if _inside_polygon(x, y, p):
            return True
    return False


def _near_any_edge(x, y, polys, delta):
    for dx, dy in ((-delta, 0), (delta, 0), (0, -delta), (0, delta)):
        if not _on_glyph(x + dx, y + dy, polys):
            return True
    return False


def render_rgba(size):
    """返回 size*size 的 RGBA 字节串（未压缩）。"""
    ss = size * SS
    radius = RADIUS_RATIO * ss
    polys = [[(px * ss, py * ss) for px, py in poly] for poly in YEN_POLYS]
    scale = float(ss) / 256.0
    edge_delta = max(1.0, 1.6 * scale) if size < 256 else 1.6

    # 先在超采样分辨率下算出覆盖信息
    acc = [[[0, 0, 0, 0] for _ in range(size)] for _ in range(size)]

    for sy in range(ss):
        ny = (sy + 0.5) / ss
        row = acc[min(int(ny * size), size - 1)]
        t = (sy + 0.5) / ss                     # 渐变参数
        bg = _blend(BG_TOP, BG_BOTTOM, t)
        for sx in range(ss):
            nx = (sx + 0.5) / ss
            cell = row[min(int(nx * size), size - 1)]
            if not _inside_rounded_rect(sx + 0.5, sy + 0.5, ss, radius):
                continue
            color = bg
            if _on_glyph(sx + 0.5, sy + 0.5, polys):
                color = GLYPH
                if _near_any_edge(sx + 0.5, sy + 0.5, polys, edge_delta):
                    color = GLYPH_EDGE
            cell[0] += color[0]
            cell[1] += color[1]
            cell[2] += color[2]
            cell[3] += 255

    per = SS * SS
    out = bytearray()
    for y in range(size):
        for x in range(size):
            r, g, b, a = acc[y][x]
            if a == 0:
                out += b"\x00\x00\x00\x00"
            else:
                # 预乘 alpha 的累计值还原为直通 alpha
                n = a // 255
                out += bytes((r // n, g // n, b // n, a // per))
    return bytes(out)


# ----------------------------------------------------------------- ICO / PNG
def _bmp_entry(size, rgba):
    """ICO 内嵌的 32bpp DIB：BITMAPINFOHEADER + 自下而上的 BGRA + AND 掩码。"""
    header = struct.pack(
        "<IiiHHIIiiII",
        40,          # biSize
        size,        # biWidth
        size * 2,    # biHeight（XOR + AND 两半）
        1,           # biPlanes
        32,          # biBitCount
        0,           # biCompression = BI_RGB
        0, 0, 0, 0, 0)
    xor = bytearray()
    for y in range(size - 1, -1, -1):
        for x in range(size):
            i = (y * size + x) * 4
            r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
            xor += bytes((b, g, r, a))
    mask_row = ((size + 31) // 32) * 4
    and_mask = bytearray()
    for y in range(size - 1, -1, -1):
        bits = bytearray(mask_row)
        for x in range(size):
            if rgba[(y * size + x) * 4 + 3] < 128:
                bits[x // 8] |= 0x80 >> (x % 8)
        and_mask += bits
    return header + bytes(xor) + bytes(and_mask)


def png_bytes(size, rgba):
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        raw += rgba[y * size * 4:(y + 1) * size * 4]

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


def build_ico(sizes, png_from=64):
    """PNG 压缩大图（>= png_from），小图用传统 DIB，兼顾体积与兼容性。"""
    images = []
    for s in sizes:
        rgba = render_rgba(s)
        images.append((s, png_bytes(s, rgba) if s >= png_from else _bmp_entry(s, rgba)))
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    offset = 6 + 16 * len(images)
    for size, data in images:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in images:
        out += data
    return bytes(out)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(root, "SalaryMonitor", "res", "salary_monitor.ico"))
    ap.add_argument("--preview", default=None)
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    data = build_ico(SIZES)
    with open(args.out, "wb") as fp:
        fp.write(data)
    print("wrote %s (%d bytes, %d sizes)" % (args.out, len(data), len(SIZES)))

    if args.preview:
        png = png_bytes(256, render_rgba(256))
        with open(args.preview, "wb") as fp:
            fp.write(png)
        print("wrote %s (%d bytes)" % (args.preview, len(png)))


if __name__ == "__main__":
    sys.exit(main())
