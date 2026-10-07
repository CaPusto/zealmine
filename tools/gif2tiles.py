#!/usr/bin/env python3
"""Converts a GIF tileset into the Zeal .zts (RLE tileset) and .ztp (palette).

Replaces gif2zeal.py, whose green channel is masked to 5 bits even though
RGB565 has 6, which darkens every mid-tone and turns the greys purple.
"""

import argparse
import os
import sys

from PIL import Image

TS = 16


def rgb565(r, g, b):
    """RGB888 -> RGB565. Green is the 6-bit field, so it shifts by 2."""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def _same_run(data):
    count = 0
    i = 0
    while i < len(data) and data[0] == data[i] and count < 128:
        count += 1
        i += 1
    return count


def _diff_run(data):
    count = 0
    i = 1
    while i < len(data) and data[i - 1] != data[i] and count < 128:
        count += 1
        i += 1
    return count


def compress(tile):
    """Byte-compatible with gif2zeal.py so the RLE decoder stays the same."""
    out = bytearray()
    i = 0
    while i < len(tile):
        same = _same_run(tile[i:])
        diff = _diff_run(tile[i:])
        if diff > 0:
            out.append(diff - 1)
            out.extend(tile[i : i + diff])
            i += diff
        else:
            out.append((same - 1) + 0x80)
            out.append(tile[i])
            i += same
    return bytes(out)


def lz77_compress(data, window=256):
    """Pack an entire tileset (concatenated 256-byte tiles) with LZ77.

    Command byte, high bit first:
      0x00-0x7F  literal run of (b + 1) raw bytes
      0x80-0xBF  run of (b & 0x3F) + 1 copies of the next byte
      0xC0-0xFF  back-ref of (b & 0x3F) + 3 bytes at offset (next byte) + 1,
                 where the offset counts back into the last 256 bytes of output

    Both RLE runs and back-refs may not cross the 256-byte tile boundary: the
    decoder hands whole tiles to the video board every time the count of decoded
    bytes reaches a multiple of TS * TS, so a command that straddled a boundary
    would desynchronise the tiles. It keeps a 256-byte circular window so a
    back-ref may still reach across the previous tile boundary's last bytes.
    """
    out = bytearray()
    n = len(data)
    pos = 0
    while pos < n:
        rem = n - pos
        rem_tile = (TS * TS) - (pos % (TS * TS))
        run = 1
        while run < min(rem, 64, rem_tile) and data[pos + run] == data[pos]:
            run += 1
        best_len = 0
        best_off = 0
        if rem >= 3:
            maxl = min(66, rem, rem_tile)
            k = pos - 1
            lo = max(0, pos - window)
            cand = 0
            while k >= lo and cand < 16:
                if data[k] == data[pos]:
                    cand += 1
                    length = 1
                    while length < maxl and data[k + length] == data[pos + length]:
                        length += 1
                    if length > best_len:
                        best_len = length
                        best_off = pos - k
                        if length >= 40:
                            break
                k -= 1
        if run >= 3 and run >= best_len - 1:
            out.append(0x80 | (min(run, 64) - 1))
            out.append(data[pos])
            pos += min(run, 64)
        elif best_len >= 3:
            out.append(0xC0 | (best_len - 3))
            out.append(best_off - 1)
            pos += best_len
        else:
            length = 1
            while (length < min(rem, 128, rem_tile)
                   and data[pos + length] != data[pos + length - 1]):
                length += 1
            out.append(length - 1)
            out.extend(data[pos : pos + length])
            pos += length
    return bytes(out)


def lz77_decompress(data):
    """Inverse of lz77_compress, used by the preview tools."""
    out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        i += 1
        if b < 0x80:
            length = b + 1
            out.extend(data[i : i + length])
            i += length
        elif b < 0xC0:
            length = (b & 0x3F) + 1
            out.extend(bytes([data[i]]) * length)
            i += 1
        else:
            length = (b & 0x3F) + 3
            offset = data[i] + 1
            i += 1
            for _ in range(length):
                out.append(out[-offset])
    return bytes(out)


def read_gif(path):
    img = Image.open(path)
    if img.mode != "P":
        sys.exit("%s is not a paletted GIF (mode %s)" % (path, img.mode))
    if img.size[1] % TS:
        sys.exit("%s height %d is not a multiple of %d" % (path, img.size[1], TS))
    if img.size[0] % TS:
        sys.exit("%s width %d is not a multiple of %d" % (path, img.size[0], TS))
    return img


def _is_blank(img, index):
    """True when the tile at `index` is entirely the transparent colour."""
    px = img.load()
    width = img.size[0]
    ox, oy = (index % (width // TS)) * TS, (index // (width // TS)) * TS
    blank = px[ox, oy]
    for y in range(TS):
        for x in range(TS):
            if px[ox + x, oy + y] != blank:
                return False
    return True


def write_tileset(img, path, strip):
    px = img.load()
    width, height = img.size
    count = (width // TS) * (height // TS)
    if strip < 0:
        # Drop the trailing tiles the sheet generator left empty, so the tileset
        # does not carry tiles the game never references.
        while count and _is_blank(img, count - 1):
            count -= 1
    elif strip:
        count = max(0, count - strip)
    data = bytearray()
    for t in range(count):
        ox, oy = (t % (width // TS)) * TS, (t // (width // TS)) * TS
        tile = [px[ox + x, oy + y] for y in range(TS) for x in range(TS)]
        data.extend(compress(tile))
    with open(path, "wb") as f:
        f.write(data)
    return count


def write_palette(img, path, colors):
    pal = img.getpalette()
    if not pal:
        sys.exit("%s has no palette" % path)
    out = bytearray()
    for i in range(colors):
        base = i * 3
        if base + 2 >= len(pal):
            break
        value = rgb565(pal[base], pal[base + 1], pal[base + 2])
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    with open(path, "wb") as f:
        f.write(out)
    return len(out) // 2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i", "--input", required=True)
    ap.add_argument("-t", "--tileset", required=True)
    ap.add_argument("-p", "--palette", required=True)
    ap.add_argument("-c", "--colors", type=int, default=256)
    ap.add_argument("-s", "--strip", type=int, default=-1,
                    help="tiles to drop from the end, or -1 to autodetect")
    args = ap.parse_args()

    img = read_gif(args.input)
    os.makedirs(os.path.dirname(os.path.abspath(args.tileset)), exist_ok=True)

    tiles = write_tileset(img, args.tileset, args.strip)
    colors = write_palette(img, args.palette, args.colors)
    print(
        "%s -> %d tiles, %d colors" % (os.path.basename(args.input), tiles, colors)
    )


if __name__ == "__main__":
    main()
