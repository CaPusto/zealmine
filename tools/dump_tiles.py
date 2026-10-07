#!/usr/bin/env python3
"""Decodes the generated .zts/.ztp asset pair and renders a preview PNG.

This checks what the game actually receives from the asset pipeline, rather
than what tools/make_tiles.py intended to produce.
"""

import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gif2tiles import lz77_decompress

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ZTS = os.path.join(ROOT, "build", "assets", "tiles.zts")
ZTP = os.path.join(ROOT, "build", "assets", "tiles.ztp")
OUT = os.path.join("/tmp/opencode", "tiles_preview.png")

TS = 16
COLS = 16
USED_TILES = 28
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


def main():
    if not os.path.exists(ZTS) or not os.path.exists(ZTP):
        sys.exit("run the cmake build first, %s is missing" % ZTS)

    palette = read_palette(ZTP)
    raw = open(ZTS, "rb").read()
    pixels = lz77_decompress(raw)

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

    print("tileset bytes : %d" % len(raw))
    print("decoded tiles : %d" % total_tiles)
    print("palette colors: %d" % len(palette))
    print("palette[0]    : %s (must never be drawn)" % (palette[0],))
    print("palette[1..11]: %s" % (palette[1:11],))

    bad = []
    for t in range(USED_TILES):
        zero = any(
            pixels[t * TS * TS + y * TS + x] == 0 for y in range(TS) for x in range(TS)
        )
        if zero and t not in (25, 26):
            bad.append(t)
    if bad:
        sys.exit("tiles %s use the transparent index outside the overlay tiles" % bad)
    print("transparent   : only tiles 25 (cursor) and 26 (blank)")
    print("wrote %s" % OUT)


if __name__ == "__main__":
    main()
