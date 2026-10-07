#!/usr/bin/env python3
"""Merges extra pictures into the board's tilesheet, tileset and palette.

tools/gif2tiles.py turns one sheet into a .zts/.ztp pair. Everything the game
paints on top of the board -- the keys hint now, the win and lose pictures
later -- has to share that palette and sit in the same tileset, because the
video board keeps a single tileset for the whole screen. So this starts from
the sheet and appends each extra picture's tiles, writing one .ztm per picture
holding the tile index every cell ended up at.

A pixel keeps its own RGB565 colour: a colour the sheet already carries reuses
that palette entry, and only a genuinely new one claims a free slot above
everything the sheet paints. Palette index 0 is the transparent colour the
video board reserves, so it is never handed out -- a picture drawn over the
board wants an opaque background anyway.

With no --overlay the output is byte for byte what gif2tiles.py produces, so
the sheet alone is still built the same way it always was.
"""

import argparse
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gif2tiles import TS, lz77_compress, read_gif, rgb565

# The tileset lives in 64 KiB of VRAM as 256 byte tiles, so 256 tiles is the
# hard ceiling a merged tileset can reach, and a tilemap cell is one byte.
MAX_TILES = 256


def sheet_cells(img):
    """The sheet as one index tuple per cell, with the trailing flat tiles
    gif2tiles.py drops still dropped, so the base tileset comes out the same."""
    px = img.load()
    width, height = img.size
    cols = width // TS
    cells = []
    for t in range(cols * (height // TS)):
        ox, oy = (t % cols) * TS, (t // cols) * TS
        cells.append(
            tuple(px[ox + x, oy + y] for y in range(TS) for x in range(TS))
        )
    while cells and all(p == cells[-1][0] for p in cells[-1]):
        cells.pop()
    return cells


def sheet_palette(img):
    """The sheet's palette as 256 RGB565 entries, padded if it runs short."""
    pal = img.getpalette() or []
    entries = []
    for i in range(256):
        base = i * 3
        if base + 2 >= len(pal):
            break
        entries.append(rgb565(pal[base], pal[base + 1], pal[base + 2]))
    entries.extend([0] * (256 - len(entries)))
    return entries


def fit_colours(img, palette, next_free):
    """Maps the picture's own palette onto the shared palette.

    Returns the index to paint for every palette index the picture uses, and
    the next unclaimed slot. Index 0 is skipped so the transparent colour is
    never overwritten with an image colour.
    """
    pal = img.getpalette() or []
    px = img.load()
    width, height = img.size
    used = sorted({px[x, y] for y in range(height) for x in range(width)})
    mapping = {}

    for index in used:
        base = index * 3
        value = rgb565(pal[base], pal[base + 1], pal[base + 2])
        slot = None
        for i in range(1, 256):
            if palette[i] == value:
                slot = i
                break
        if slot is None:
            if next_free >= MAX_TILES:
                sys.exit("%s needs more palette slots than the palette holds"
                         % img.filename)
            slot = next_free
            palette[slot] = value
            next_free += 1
        mapping[index] = slot
    return mapping, next_free


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i", "--input", required=True, help="base tilesheet")
    ap.add_argument("-t", "--tileset", required=True)
    ap.add_argument("-p", "--palette", required=True)
    ap.add_argument("-o", "--overlay", action="append", default=[],
                    help="picture to merge in; repeatable")
    ap.add_argument("-m", "--overlay-map", action="append", default=[],
                    help="tilemap to write for the matching --overlay")
    args = ap.parse_args()

    if len(args.overlay) != len(args.overlay_map):
        sys.exit("every --overlay needs its own --overlay-map")

    sheet = read_gif(args.input)
    palette = sheet_palette(sheet)
    base = sheet_cells(sheet)

    taken = {i for tile in base for i in tile}
    next_free = max(taken) + 1 if taken else 1

    known = {}
    for index, tile in enumerate(base):
        known.setdefault(tile, index)

    extra = []
    reports = []

    for path, out in zip(args.overlay, args.overlay_map):
        img = read_gif(path)
        mapping, next_free = fit_colours(img, palette, next_free)

        px = img.load()
        width, height = img.size
        cols, rows = width // TS, height // TS
        added = 0
        cells = []
        for ty in range(rows):
            for tx in range(cols):
                tile = tuple(
                    mapping[px[tx * TS + x, ty * TS + y]]
                    for y in range(TS) for x in range(TS)
                )
                if tile not in known:
                    if len(base) + len(extra) + 1 > MAX_TILES:
                        sys.exit("%s would push the tileset past %d tiles"
                                 % (img.filename, MAX_TILES))
                    known[tile] = len(base) + len(extra)
                    extra.append(tile)
                    added += 1
                cells.append(known[tile])

        with open(out, "wb") as f:
            f.write(bytes(cells))
        reports.append("%s -> %dx%d cells, %d tiles added, %d bytes in VRAM"
                       % (os.path.basename(path), cols, rows, added,
                          len(extra) * TS * TS))

    tiles = base + extra
    data = lz77_compress(b"".join(bytes(t) for t in tiles))
    if len(data) > 0xFFFF:
        sys.exit("%s would need %d bytes, more than the 16-bit size argument"
                 % (args.tileset, len(data)))

    os.makedirs(os.path.dirname(os.path.abspath(args.tileset)), exist_ok=True)
    with open(args.tileset, "wb") as f:
        f.write(data)

    out = bytearray()
    for value in palette:
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    with open(args.palette, "wb") as f:
        f.write(out)

    print("%s -> %d base tiles, %d merged, %d colors"
          % (os.path.basename(args.input), len(base), len(tiles), len(out) // 2))
    for line in reports:
        print("  " + line)


if __name__ == "__main__":
    main()
