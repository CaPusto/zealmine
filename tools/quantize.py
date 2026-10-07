#!/usr/bin/env python3
"""Brings a picture's colour count down until it fits the shared palette.

The board keeps a single 256-slot palette and one tileset for the whole
screen, so every merged picture must share it. Pictures with close to 256
colours (dithering-heavy GIFs) can exceed the slots still free after the
board, the keys hint, the banner and the divider are counted; this rewrites
one to a lower colour count before tools/gif2image.py merges it.
"""

import argparse

from PIL import Image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-i", "--input", required=True)
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("-n", "--colors", type=int, default=220)
    args = ap.parse_args()

    im = Image.open(args.input).convert("RGB")
    quant = im.quantize(
        colors=args.colors,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.FLOYDSTEINBERG,
    )
    quant.save(args.output, "GIF")
    print("%s -> %d colours" % (args.output, len(quant.getpalette()) // 3))


if __name__ == "__main__":
    main()