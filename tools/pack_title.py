#!/usr/bin/env python3
"""Pack SDK title assets into the game's blank-tile-prefixed LZ77 format.

SDK gif2zeal owns palette conversion, exact tile deduplication and tilemap
creation. Artwork must already fit; this step never modifies image pixels.
"""

import argparse
from pathlib import Path

from tileset_lz77 import TS, lz77_compress

# Preserve one transparent blank tile at index 0 and one spare VRAM slot.
MAX_ART_TILES = 254
TITLE_CELLS = 20 * 15


def pack_title(source, output):
    tiles = source.with_suffix(".zts").read_bytes()
    palette = source.with_suffix(".ztp").read_bytes()
    tilemap = source.with_suffix(".ztm").read_bytes()
    tile_size = TS * TS // 2
    count = len(tiles) // tile_size
    if len(tiles) % tile_size or not 1 <= count <= MAX_ART_TILES:
        raise ValueError("title must contain 1..%d whole raw 4bpp tiles"
                         % MAX_ART_TILES)
    if len(palette) != 32:
        raise ValueError("title must have a 16-entry RGB565 palette")
    if len(tilemap) != TITLE_CELLS or max(tilemap) >= count:
        raise ValueError("title must have a valid 20x15 tilemap")
    if any(byte >> 4 == 0 or byte & 0x0F == 0 for byte in tiles):
        raise ValueError("title artwork must reserve palette index 0 for transparency")

    packed = lz77_compress(bytes(tile_size) + tiles, tile_size=tile_size)
    if len(packed) > 0xFFFF:
        raise ValueError("compressed title exceeds the 16-bit loader size")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.with_suffix(".zts").write_bytes(packed)
    output.with_suffix(".ztp").write_bytes(palette)
    output.with_suffix(".ztm").write_bytes(bytes(index + 1 for index in tilemap))
    print("%d artwork tiles + blank, %d compressed bytes, %d map cells"
          % (count, len(packed), len(tilemap)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", type=Path, required=True,
                        help="SDK asset prefix (without extension)")
    parser.add_argument("-o", "--output", type=Path, required=True,
                        help="packed asset prefix (without extension)")
    args = parser.parse_args()
    try:
        pack_title(args.input, args.output)
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
