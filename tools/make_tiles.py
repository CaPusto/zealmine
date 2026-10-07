#!/usr/bin/env python3
"""Builds assets/tiles.gif.

Palette index 0 is reserved and never drawn: on the Zeal foreground layer
colour 0 is always transparent, so it lets layer 1 overlay the field without
covering it.
"""

import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "assets", "tiles_src.gif")
OUT = os.path.join(ROOT, "assets", "tiles.gif")

TS = 16
COLS = 16
ROWS = 3
USED_TILES = 28
TRANSPARENT = 0
MASK = 0xFF

P_TRANSPARENT = 0
P_LIGHT = 1
P_MID = 2
P_WHITE = 3
P_BLACK = 4
P_RED = 5
P_BLUE = 6
P_GREEN = 7
P_NAVY = 8
P_DRED = 9
P_TEAL = 10

PALETTE_RGB = {
    P_TRANSPARENT: (1, 0, 1),
    P_LIGHT: (192, 192, 192),
    P_MID: (128, 128, 128),
    P_WHITE: (255, 255, 255),
    P_BLACK: (0, 0, 0),
    P_RED: (255, 0, 0),
    P_BLUE: (0, 0, 255),
    P_GREEN: (0, 128, 0),
    P_NAVY: (0, 0, 128),
    P_DRED: (128, 0, 0),
    P_TEAL: (0, 128, 128),
}

SEGMENTS = {
    "0": "abcdef",
    "1": "bc",
    "2": "abged",
    "3": "abgcd",
    "4": "fgbc",
    "5": "afgcd",
    "6": "afgecd",
    "7": "abc",
    "8": "abcdefg",
    "9": "abfgcd",
}

SEG_GEOM = {
    "a": (4, 1, 11, 2),
    "b": (12, 2, 13, 6),
    "c": (12, 9, 13, 13),
    "d": (4, 13, 11, 14),
    "e": (2, 9, 3, 13),
    "f": (2, 2, 3, 6),
    "g": (4, 7, 11, 8),
}

class Canvas:
    def __init__(self, width, height):
        self.w = width
        self.h = height
        self.px = bytearray([P_TRANSPARENT]) * (width * height)

    def set(self, x, y, color):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = color

    def rect(self, x0, y0, x1, y1, color):
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.set(x, y, color)

    def fill(self, x0, y0, x1, y1, color):
        self.rect(x0, y0, x1, y1, color)

    def hline(self, x0, x1, y, color):
        self.rect(x0, y, x1, y, color)

    def vline(self, x, y0, y1, color):
        self.rect(x, y0, x, y1, color)

    def disc(self, cx, cy, r, color):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r:
                    self.set(x, y, color)

    def polygon(self, points, color):
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        for y in range(min(ys), max(ys) + 1):
            crossings = []
            for i in range(len(points)):
                x1, y1 = points[i]
                x2, y2 = points[(i + 1) % len(points)]
                if y1 == y2:
                    continue
                if min(y1, y2) <= y < max(y1, y2):
                    t = (y - y1) / float(y2 - y1)
                    crossings.append(x1 + t * (x2 - x1))
            crossings.sort()
            for i in range(0, len(crossings) - 1, 2):
                self.rect(
                    int(round(crossings[i])), y, int(round(crossings[i + 1])), y, color
                )

    def blit(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                self.set(ox + x, oy + y, other.px[y * other.w + x])


def raised(c, ox, oy):
    c.rect(ox, oy, ox + TS - 1, oy + TS - 1, P_LIGHT)
    c.hline(ox, ox + TS - 2, oy, P_WHITE)
    c.vline(ox, oy, oy + TS - 2, P_WHITE)
    c.hline(ox + 1, ox + TS - 1, oy + TS - 1, P_MID)
    c.vline(ox + TS - 1, oy + 1, oy + TS - 1, P_MID)


def sunken(c, ox, oy):
    c.rect(ox, oy, ox + TS - 1, oy + TS - 1, P_LIGHT)
    c.hline(ox, ox + TS - 1, oy, P_MID)
    c.vline(ox, oy, oy + TS - 1, P_MID)


def digit(c, ox, oy, ch, color=P_RED):
    sunken(c, ox, oy)
    for seg in SEGMENTS[ch]:
        x0, y0, x1, y1 = SEG_GEOM[seg]
        c.rect(ox + x0, oy + y0, ox + x1, oy + y1, color)


def mine(c, ox, oy, spike, cross):
    sunken(c, ox, oy)
    cx, cy, r = ox + 8, oy + 8, 5
    c.vline(cx, cy - r - 2, cy - r + 1, spike)
    c.vline(cx, cy + r - 1, cy + r + 2, spike)
    c.hline(cx - r - 2, cx - r + 1, cy, spike)
    c.hline(cx + r - 1, cx + r + 2, cy, spike)
    c.disc(cx, cy, r, P_BLACK)
    c.disc(cx - 3, cy - 3, 1, P_WHITE)
    if cross:
        for i in range(-3, 4):
            c.set(cx + i, cy + i, P_RED)
            c.set(cx - i, cy + i, P_RED)


def tile_covered(c, ox, oy):
    raised(c, ox, oy)


def tile_flag(c, ox, oy):
    raised(c, ox, oy)
    c.vline(ox + 7, oy + 3, oy + 12, P_BLACK)
    c.polygon(
        [(ox + 8, oy + 3), (ox + 13, oy + 4), (ox + 13, oy + 8), (ox + 8, oy + 9)],
        P_RED,
    )
    c.hline(ox + 5, ox + 11, oy + 13, P_BLACK)


def tile_minus(c, ox, oy):
    sunken(c, ox, oy)
    c.rect(ox + 4, oy + 7, ox + 11, oy + 8, P_RED)


def tile_colon(c, ox, oy):
    sunken(c, ox, oy)
    c.rect(ox + 7, oy + 4, ox + 8, oy + 5, P_RED)
    c.rect(ox + 7, oy + 10, ox + 8, oy + 11, P_RED)


def tile_cursor(c, ox, oy):
    c.fill(ox, oy, ox + TS - 1, oy + TS - 1, P_TRANSPARENT)
    c.hline(ox, ox + TS - 1, oy, P_BLACK)
    c.hline(ox, ox + TS - 1, oy + TS - 1, P_BLACK)
    c.vline(ox, oy, oy + TS - 1, P_BLACK)
    c.vline(ox + TS - 1, oy, oy + TS - 1, P_BLACK)


def tile_blank(c, ox, oy):
    c.fill(ox, oy, ox + TS - 1, oy + TS - 1, P_TRANSPARENT)


RGB_TO_INDEX = {rgb: idx for idx, rgb in PALETTE_RGB.items()}


def rgb_to_index(rgb):
    if rgb not in RGB_TO_INDEX:
        sys.exit("source color %s is not in the palette" % (rgb,))
    return RGB_TO_INDEX[rgb]


def load_source():
    sheet = Image.open(SRC).convert("RGB")
    if sheet.size != (TS * COLS, TS):
        sys.exit("unexpected source size %s" % (sheet.size,))

    strip = Canvas(sheet.size[0], sheet.size[1])
    src = sheet.load()
    for y in range(sheet.size[1]):
        for x in range(sheet.size[0]):
            strip.set(x, y, rgb_to_index(src[x, y]))
    return strip


def build():
    strip = load_source()
    canvas = Canvas(TS * COLS, TS * ROWS)
    canvas.blit(strip, 0, 0)

    def at(i):
        return ((i % COLS) * TS, (i // COLS) * TS)

    # The number tiles at the head of the sheet are the digits 1..8 in natural
    # order, each in its classic minesweeper colour, so render.c's
    # NUMBER_TILES[] (TILE_NUM1..8 in src/zealmine.h) reaches them by counting
    # directly. The hand-drawn digits they replace read ambiguously at 16x16;
    # the same segments as the HUD digits below keep every value unmistakable.
    # The sheet is kept compact: every tile is used, and the unused source
    # tiles (the mine-hint, the bare question mark and the decorative tiles
    # 13..15) are not carried over.
    DIGIT_COLORS = [P_BLUE, P_GREEN, P_RED, P_NAVY, P_DRED, P_TEAL, P_BLACK, P_MID]
    for i, ch in enumerate("12345678"):
        digit(canvas, *at(i), ch, DIGIT_COLORS[i])
    tile_colon(canvas, *at(10))
    tile_covered(canvas, *at(11))
    tile_flag(canvas, *at(12))
    mine(canvas, *at(13), P_BLACK, False)
    mine(canvas, *at(14), P_RED, False)
    mine(canvas, *at(15), P_BLACK, True)
    # The HUD digits 0..9. The "3" glyph is byte for byte the number tile 2
    # (both are red), so it is not carried as its own cell: render.c draws it
    # with TILE_NUM3 and the remaining digits pack into 16..24.
    for i, ch in enumerate("012"):
        digit(canvas, *at(16 + i), ch)
    for i, ch in enumerate("456789"):
        digit(canvas, *at(19 + i), ch)
    tile_cursor(canvas, *at(25))
    tile_blank(canvas, *at(26))
    tile_minus(canvas, *at(27))

    img = Image.new("P", (canvas.w, canvas.h))
    img.putpalette(
        [c for i in range(256) for c in PALETTE_RGB.get(i, (0, 0, 0))]
    )
    img.frombytes(bytes(canvas.px))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT, "GIF", optimize=False)
    return canvas


def verify_saved():
    img = Image.open(OUT)
    if img.size != (TS * COLS, TS * ROWS):
        sys.exit("saved image has wrong size %s" % (img.size,))
    pal = img.getpalette()
    for idx, rgb in PALETTE_RGB.items():
        got = tuple(pal[idx * 3 : idx * 3 + 3])
        if got != rgb:
            sys.exit("palette[%d] is %s, expected %s" % (idx, got, rgb))
    px = img.load()
    for t in range(USED_TILES):
        ox, oy = (t % COLS) * TS, (t // COLS) * TS
        used = {px[ox + x, oy + y] for y in range(TS) for x in range(TS)}
        if TRANSPARENT in used and t not in (25, 26):
            sys.exit("tile %d uses transparent pixels" % t)


def verify(canvas):
    for t in range(USED_TILES):
        ox, oy = (t % COLS) * TS, (t // COLS) * TS
        tile = {
            canvas.px[(oy + y) * canvas.w + ox + x]
            for y in range(TS)
            for x in range(TS)
        }
        if TRANSPARENT in tile and t not in (25, 26):
            sys.exit("tile %d contains transparent pixels" % t)
    verify_saved()


if __name__ == "__main__":
    c = build()
    verify(c)
    print(
        "wrote %s (%dx%d, %d tiles used)" % (OUT, c.w, c.h, USED_TILES)
    )
