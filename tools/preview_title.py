#!/usr/bin/env python3
"""Reassembles the generated title assets into a PNG, to check the conversion.

The video board has no framebuffer mode: it shows tiles. So the title is stored
as a palette, an LZ77-compressed tileset and a byte per cell tilemap, and this
script undoes the conversion the way the hardware does, so the result can be
compared with the source GIF before flashing anything.
"""

import argparse
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gif2tiles import lz77_decompress

TS = 16
COLS = 20
ROWS = 15


def decode_rle(data):
    """Same format src/tileset_lz77.c's tileset_load_lz77() reads."""
    return lz77_decompress(data)


def rgb565(value):
    """Expand a 16-bit RGB565 entry back to 8 bits per channel."""
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31)


def render(assets):
    palette = open(os.path.join(assets, "title.ztp"), "rb").read()
    tiles = decode_rle(open(os.path.join(assets, "title.zts"), "rb").read())
    tilemap = open(os.path.join(assets, "title.ztm"), "rb").read()

    if len(tiles) % (TS * TS):
        sys.exit("tileset is %d bytes, not a whole number of tiles" % len(tiles))
    if max(tilemap) * TS * TS >= len(tiles):
        sys.exit("tilemap points at tile %d, past the end of the tileset"
                 % max(tilemap))

    img = Image.new("RGB", (COLS * TS, ROWS * TS))
    px = img.load()
    for y in range(ROWS):
        for x in range(COLS):
            base = tilemap[y * COLS + x] * TS * TS
            for j in range(TS):
                for i in range(TS):
                    index = tiles[base + j * TS + i]
                    px[x * TS + i, y * TS + j] = rgb565(
                        palette[index * 2] | (palette[index * 2 + 1] << 8))
    return img


def compare(rendered, source):
    a = rendered.load()
    b = source.convert("RGB").load()
    if rendered.size != source.size:
        sys.exit("size mismatch %s vs %s" % (rendered.size, source.size))
    hard = []
    for y in range(source.size[1]):
        for x in range(source.size[0]):
            # Below this, the difference is just the board's RGB565 rounding.
            if max(abs(a[x, y][i] - b[x, y][i]) for i in range(3)) > 8:
                hard.append((x, y))
    return hard


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("-a", "--assets", default=os.path.join(here, "build", "assets"))
    ap.add_argument("-i", "--input", default=os.path.join(here, "assets", "zeal_mine_title.gif"))
    ap.add_argument("-o", "--output", default=None)
    args = ap.parse_args()

    img = render(args.assets)
    out = args.output or os.path.join(args.assets, "title_preview.png")
    img.save(out)

    hard = compare(img, Image.open(args.input))
    print("wrote %s, %d pixels differ by more than RGB565 rounding (%d total)"
          % (out, len(hard), img.size[0] * img.size[1]))
    for x, y in hard:
        print("  %d,%d" % (x, y))


if __name__ == "__main__":
    main()