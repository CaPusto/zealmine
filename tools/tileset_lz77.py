"""Repo-specific tileset codec shared by asset builders and previews."""

import argparse
from pathlib import Path

TS = 16


def lz77_compress(data, window=256, tile_size=TS * TS):
    """Pack a tileset with LZ77; tile_size is 256 for 8bpp, 128 for 4bpp.

    Command byte, high bit first:
      0x00-0x7F  literal run of (b + 1) raw bytes
      0x80-0xBF  run of (b & 0x3F) + 1 copies of the next byte
      0xC0-0xFF  back-ref of (b & 0x3F) + 3 bytes at offset (next byte) + 1,
                 where the offset counts back into the last 256 bytes of output

    Commands may not cross tile_size boundaries: the decoder uploads each
    complete tile through the SDK, so a command that straddled a boundary
    would desynchronise the tiles. It keeps a 256-byte circular window so a
    back-ref may still reach across the previous tile boundary's last bytes.
    """
    out = bytearray()
    n = len(data)
    pos = 0
    while pos < n:
        rem = n - pos
        rem_tile = tile_size - (pos % tile_size)
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


def main():
    """Concatenate SDK raw tilesets of one bit depth, then compress."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", required=True, type=Path, nargs="+",
                        help="raw tilesets, in VRAM order")
    parser.add_argument("--tile-size", type=int, choices=(128, 256), default=256)
    parser.add_argument("-o", required=True, type=Path, help="LZ77 output tileset")
    args = parser.parse_args()
    try:
        inputs = []
        for path in args.i:
            data = path.read_bytes()
            if not data:
                parser.error(f"{path}: input tileset is empty")
            if len(data) % args.tile_size:
                parser.error(f"{path}: input must contain whole {args.tile_size}-byte tiles")
            inputs.append(data)
        data = b"".join(inputs)
        if len(data) > 256 * args.tile_size:
            parser.error("input exceeds 256 tiles")
        packed = lz77_compress(data, tile_size=args.tile_size)
        if len(packed) > 0xFFFF:
            parser.error("compressed output exceeds 65,535 bytes")
        args.o.write_bytes(packed)
    except OSError as error:
        parser.exit(1, f"{error}\n")


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


if __name__ == "__main__":
    main()


