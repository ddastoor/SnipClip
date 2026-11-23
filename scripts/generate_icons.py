#!/usr/bin/env python3
"""Deterministic pure-Python PNG icon generator for SnipClip hicolor & pixmap themes."""

from __future__ import annotations

import math
import struct
import sys
import zlib
from pathlib import Path
from typing import Tuple

RGBA = Tuple[int, int, int, int]


def _dist_to_segment(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    dx = bx - ax
    dy = by - ay
    l2 = dx * dx + dy * dy
    if l2 == 0.0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _rounded_rect_sdf(px: float, py: float, cx: float, cy: float, hw: float, hh: float, r: float) -> float:
    qx = abs(px - cx) - (hw - r)
    qy = abs(py - cy) - (hh - r)
    return math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r


def _sample_icon_128(ux: float, uy: float) -> RGBA:
    """Sample SnipClip icon in 128x128 logical coordinate space."""
    # 1. Rounded dark slate badge (#0F172A) at x=8..120, y=8..120, rx=24
    badge_d = _rounded_rect_sdf(ux, uy, 64.0, 64.0, 56.0, 56.0, 24.0)
    if badge_d > 0.6:
        return (0, 0, 0, 0)

    bg_alpha = 255 if badge_d < -0.6 else int(round(255.0 * (0.6 - badge_d) / 1.2))
    r, g, b = 15, 23, 42  # #0F172A

    # 2. Dashed cyan capture frame (#38BDF8) at x=26..86, y=26..74, rx=6
    frame_d = abs(_rounded_rect_sdf(ux, uy, 56.0, 50.0, 30.0, 24.0, 6.0))
    if frame_d <= 2.7:
        perim_phase = int((ux + uy) // 8.0) % 2
        if perim_phase == 0:
            r, g, b = 56, 189, 248  # #38BDF8

    # 3. White scissors (#F8FAFC)
    ring1_d = abs(math.hypot(ux - 80.0, uy - 88.0) - 10.5)
    ring2_d = abs(math.hypot(ux - 52.0, uy - 94.0) - 10.5)
    blade1_d = _dist_to_segment(ux, uy, 60.0, 86.0, 98.0, 48.0)
    blade2_d = _dist_to_segment(ux, uy, 72.0, 80.0, 44.0, 52.0)

    fg_d = min(ring1_d, ring2_d, blade1_d, blade2_d)
    if fg_d <= 2.8:
        t = 1.0 if fg_d <= 1.8 else (2.8 - fg_d)
        r = int(round(r * (1.0 - t) + 248.0 * t))
        g = int(round(g * (1.0 - t) + 250.0 * t))
        b = int(round(b * (1.0 - t) + 252.0 * t))

    return (r, g, b, bg_alpha)


def render_png_bytes(size: int) -> bytes:
    """Render an anti-aliased RGBA PNG of the given pixel width/height."""
    scale = 128.0 / float(size)
    offsets = (0.25, 0.75)
    raw_rows = bytearray()

    for py in range(size):
        raw_rows.append(0)  # PNG filter type 0 (None)
        for px in range(size):
            acc_r = 0
            acc_g = 0
            acc_b = 0
            acc_a = 0
            for oy in offsets:
                uy = (py + oy) * scale
                for ox in offsets:
                    ux = (px + ox) * scale
                    sr, sg, sb, sa = _sample_icon_128(ux, uy)
                    acc_r += sr * sa
                    acc_g += sg * sa
                    acc_b += sb * sa
                    acc_a += sa
            out_a = acc_a // 4
            if out_a > 0:
                out_r = min(255, acc_r // acc_a)
                out_g = min(255, acc_g // acc_a)
                out_b = min(255, acc_b // acc_a)
            else:
                out_r = out_g = out_b = 0
            raw_rows.extend((out_r, out_g, out_b, out_a))

    def _chunk(chunk_type: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows), level=9)
    return b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", idat) + _chunk(b"IEND", b"")


def main() -> int:
    if len(sys.argv) != 2:
        sys.stderr.write("Usage: generate_icons.py <stage_dir>\n")
        return 1

    stage_dir = Path(sys.argv[1])
    sizes = (16, 24, 32, 48, 64, 128, 256)
    rendered = {sz: render_png_bytes(sz) for sz in sizes}

    for sz, png_data in rendered.items():
        icon_dir = stage_dir / "usr" / "share" / "icons" / "hicolor" / f"{sz}x{sz}" / "apps"
        icon_dir.mkdir(parents=True, exist_ok=True)
        out_file = icon_dir / "snipclip.png"
        out_file.write_bytes(png_data)
        out_file.chmod(0o644)

    pixmaps_dir = stage_dir / "usr" / "share" / "pixmaps"
    pixmaps_dir.mkdir(parents=True, exist_ok=True)
    pixmap_file = pixmaps_dir / "snipclip.png"
    pixmap_file.write_bytes(rendered[64])
    pixmap_file.chmod(0o644)

    app_share_dir = stage_dir / "usr" / "share" / "snipclip"
    app_share_dir.mkdir(parents=True, exist_ok=True)
    app_icon_file = app_share_dir / "snipclip.png"
    app_icon_file.write_bytes(rendered[128])
    app_icon_file.chmod(0o644)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
