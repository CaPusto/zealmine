"""Repo-specific tileset codec shared by asset builders and previews."""

TS = 16


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


