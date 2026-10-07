#!/usr/bin/env python3
"""Converts a full screen title GIF into the Zeal .ztp/.zts/.ztm triplet.

Unlike tools/gif2tiles.py, which expects a tilesheet and keeps every tile as it
is, this builds a tilemap: the screen is sliced into 16x16 tiles, identical
tiles are shared, and the result is written out as a palette, an RLE tileset and
a byte per cell tilemap.

The Zeal 8-bit Video Board keeps the tileset in 64 KiB of VRAM, which is 256
tiles of 256 bytes. A 320x240 screen needs 300 tiles, so a detailed image runs
out of room. When the deduplicated image does not fit in MAX_TILES, the closest
tile pairs are merged one by one, cheapest pair first, so the few damaged spots
land on pairs that were nearly identical to begin with.
"""

import argparse
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gif2tiles import TS, lz77_compress

# Palette index 0 is the one the video board treats as transparent, so it is kept
# out of the image and carries a spare fully transparent tile instead. The HUD,
# board and cursor live on layer 1 and need somewhere blank to hide behind.
TRANSPARENT_RGB = (1, 0, 1)
BLANK_TILE = 0

# A layer cell is a single byte and the tileset window holds 64 KiB of 256 byte
# tiles, so 256 is the hard ceiling whatever the video mode allows: the SDK
# itself fits 64 tiles per 16 KiB page in 8bpp. One of those slots goes to the
# blank tile and one is left spare.
TILE_INDEX_MAX = 256
MAX_TILES = 254


def to_rgb565(r, g, b):
    """RGB888 to RGB565, rounded to the nearest step instead of truncated."""
    return ((r * 31 + 127) // 255 << 11) | ((g * 63 + 127) // 255 << 5) | ((b * 31 + 127) // 255)


def read_gif(path):
    img = Image.open(path)
    if img.size[1] % TS or img.size[0] % TS:
        sys.exit("%s must be a multiple of %d" % (path, TS))
    return img.convert("RGB")


def build_palette(img):
    """Palette entries by pixel count, behind the transparent color at index 0."""
    counts = img.getcolors(maxcolors=1 << 16)
    if counts is None:
        sys.exit("%s has more than 65536 colors" % img.filename)
    if len(counts) > TILE_INDEX_MAX - 1:
        sys.exit("%s needs %d colors, the palette holds %d"
                 % (img.filename, len(counts), TILE_INDEX_MAX - 1))
    ordered = [TRANSPARENT_RGB]
    ordered += [rgb for _, rgb in sorted(counts, key=lambda c: -c[0])]
    return ordered, {rgb: i for i, rgb in enumerate(ordered)}


def slice_tiles(img, palette):
    px = img.load()
    width, height = img.size
    rows, cols = height // TS, width // TS
    tiles = []
    for ty in range(rows):
        for tx in range(cols):
            tiles.append(
                tuple(
                    palette[px[tx * TS + x, ty * TS + y]]
                    for y in range(TS)
                    for x in range(TS)
                )
            )
    return tiles, cols, rows


def find_root(roots, i):
    while roots[i] != i:
        roots[i] = roots[roots[i]]
        i = roots[i]
    return i


def count_roots(roots):
    return len({find_root(roots, i) for i in range(len(roots))})


def merge_closest(unique, limit):
    """Merge the cheapest tile pairs until at most `limit` tiles are left.

    Returns the surviving index for every tile of `unique`, as a parent array
    that find_root() resolves.
    """
    n = len(unique)
    dist = [[0] * n for _ in range(n)]
    for i in range(n):
        a = unique[i]
        for j in range(i + 1, n):
            b = unique[j]
            d = sum(1 for k in range(TS * TS) if a[k] != b[k])
            dist[i][j] = d
            dist[j][i] = d

    roots = list(range(n))
    while count_roots(roots) > limit:
        best = None
        for i in range(n):
            if find_root(roots, i) != i:
                continue
            for j in range(i + 1, n):
                if find_root(roots, j) != j:
                    continue
                if best is None or dist[i][j] < best[0]:
                    best = (dist[i][j], i, j)
        if best is None:
            break
        _, keep, drop = best
        # Distances shrink to the survivor, so the next merge is judged against
        # the tiles that are actually still on screen.
        for k in range(n):
            if find_root(roots, k) != k or k in (keep, drop):
                continue
            if dist[drop][k] < dist[keep][k]:
                dist[keep][k] = dist[drop][k]
                dist[k][keep] = dist[drop][k]
        roots[drop] = keep
        print("  merged tile %d into %d, %d pixels apart" % (drop, keep, best[0]))
    return roots


def dedup(tiles, limit):
    """Shares identical tiles, then trims the set down to `limit` tiles.

    Returns the tiles to store, for every cell of the screen the index of the
    tile it now uses, and how many distinct tiles there were to begin with.
    """
    if limit > TILE_INDEX_MAX:
        sys.exit("a tilemap cell holds one byte, so %d tiles is the most"
                 % TILE_INDEX_MAX)

    unique = []
    first = {}
    for t in tiles:
        if t not in first:
            first[t] = len(unique)
            unique.append(t)
    raw = len(unique)

    if len(unique) > limit:
        roots = merge_closest(unique, limit)
    else:
        roots = list(range(len(unique)))

    order = []
    remap = {}
    for i in range(len(unique)):
        r = find_root(roots, i)
        if r not in remap:
            remap[r] = len(order)
            order.append(r)

    kept = [unique[r] for r in order]
    cells = [remap[find_root(roots, first[t])] for t in tiles]
    return kept, cells, raw


def write_tileset(tiles, path):
    data = lz77_compress(b"".join(bytes(t) for t in tiles))
    if len(data) > 0xFFFF:
        sys.exit("%s would need %d bytes, more than the 16-bit size argument"
                 % (path, len(data)))
    with open(path, "wb") as f:
        f.write(data)
    return len(data)


def write_palette(ordered, path):
    out = bytearray()
    for i in range(256):
        r, g, b = ordered[i] if i < len(ordered) else (0, 0, 0)
        value = to_rgb565(r, g, b)
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    with open(path, "wb") as f:
        f.write(out)
    return len(out) // 2


def write_tilemap(mapping, path):
    out = bytearray()
    for i in range(len(mapping)):
        out.append(mapping[i])
    with open(path, "wb") as f:
        f.write(out)
    return len(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i", "--input", required=True)
    ap.add_argument("-p", "--palette", required=True)
    ap.add_argument("-t", "--tileset", required=True)
    ap.add_argument("-m", "--tilemap", required=True)
    args = ap.parse_args()

    img = read_gif(args.input)
    ordered, palette = build_palette(img)
    tiles, cols, rows = slice_tiles(img, palette)
    unique, mapping, raw = dedup(tiles, MAX_TILES)
    # Tile 0 is the blank layer 1 is cleared with, so the art starts at 1.
    unique.insert(BLANK_TILE, (0,) * (TS * TS))
    mapping = [i + 1 for i in mapping]

    os.makedirs(os.path.dirname(os.path.abspath(args.tileset)), exist_ok=True)
    write_tileset(unique, args.tileset)
    write_palette(ordered, args.palette)
    cells = write_tilemap(mapping, args.tilemap)
    print("%s -> %dx%d, %d colors + 1 transparent,"
          " %d tiles stored for %d distinct tiles, %d bytes in VRAM,"
          " %d tilemap cells"
          % (os.path.basename(args.input), img.size[0], img.size[1],
             len(ordered) - 1, len(unique), raw, len(unique) * TS * TS, cells))


if __name__ == "__main__":
    main()