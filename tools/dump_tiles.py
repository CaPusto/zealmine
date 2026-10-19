#!/usr/bin/env python3
"""Decodes the generated .zts/.ztp asset pair and renders a preview PNG.

This checks the tiles and palette the game receives from the asset pipeline.
"""

import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tileset_lz77 import lz77_decompress

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join("/tmp/opencode", "tiles_preview.png")

TS = 16
COLS = 16
USED_TILES = 57
TILEMAP_COLS = 80


def read_palette(path):
    raw = open(path, "rb").read()
    colors = []
    for i in range(0, len(raw) - 1, 2):
        lo, hi = raw[i], raw[i + 1]
        v = (hi << 8) | lo
        r = ((v >> 11) & 0x1F) << 3
        g = ((v >> 5) & 0x3F) << 2
        b = (v & 0x1F) << 3
        colors.append((r, g, b))
    return colors


def load_gameplay_assets(directory):
    """Reproduce SDK expansion and palette offsets for gameplay previews."""
    from pathlib import Path
    directory = Path(directory)
    pixels, palette, offsets = bytearray(), bytearray(), {}
    for name in ("soldier", "board", "divider"):
        offsets[name] = len(palette) // 2
        palette.extend((directory / f"{name}_tiles.ztp").read_bytes())
    for name, packed in [("board", True), ("divider", True), ("soldier", False)]:
        raw = lz77_decompress((directory / f"{name}.zts").read_bytes())
        if packed:
            raw = bytes(n for byte in raw for n in (byte >> 4, byte & 15))
        pixels.extend(index + offsets[name] if index else 0 for index in raw)
    return bytes(pixels), bytes(palette)


def main():
    pixels, palette_bytes = load_gameplay_assets(os.path.join(ROOT, "build", "assets"))
    import struct
    palette = []
    for (v,) in struct.iter_unpack("<H", palette_bytes):
        palette.append((((v >> 11) & 31) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3))

    total_tiles = len(pixels) // (TS * TS)
    if total_tiles < USED_TILES:
        sys.exit("tileset has only %d tiles" % total_tiles)

    sheet = Image.new("RGB", (COLS * TS, ((USED_TILES - 1) // COLS + 1) * TS))
    for t in range(USED_TILES):
        ox, oy = (t % COLS) * TS, (t // COLS) * TS
        for y in range(TS):
            for x in range(TS):
                idx = pixels[t * TS * TS + y * TS + x]
                if idx >= len(palette):
                    sys.exit("pixel index %d out of palette" % idx)
                sheet.putpixel((ox + x, oy + y), palette[idx])

    d = ImageDraw.Draw(sheet)
    for t in range(USED_TILES):
        ox, oy = (t % COLS) * TS, (t // COLS) * TS
        d.rectangle([ox, oy, ox + TS - 1, oy + TS - 1], outline=(255, 0, 255))
    sheet.save(OUT)

    print("tileset bytes : %d" % sum(os.path.getsize(os.path.join(ROOT, "build", "assets", n + ".zts")) for n in ("board", "divider", "soldier")))
    print("decoded tiles : %d" % total_tiles)
    print("palette colors: %d" % len(palette))
    print("palette[0]    : %s (must never be drawn)" % (palette[0],))
    print("palette[1..11]: %s" % (palette[1:11],))

    bad = []
    for t in range(USED_TILES):
        zero = any(
            pixels[t * TS * TS + y * TS + x] == 0 for y in range(TS) for x in range(TS)
        )
        if zero and t not in (54, 55):
            bad.append(t)
    if bad:
        sys.exit("tiles %s use the transparent index outside the overlay tiles" % bad)
    print("transparent   : only tiles 54 (cursor) and 55 (blank)")
    print("wrote %s" % OUT)


if __name__ == "__main__":
    main()
