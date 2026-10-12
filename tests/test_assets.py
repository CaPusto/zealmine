"""Run after `zde cmake`: python3 -m unittest discover -s tests -p test_assets.py.

Picture and semantic tile hashes preserve RGB565 colors and transparency,
independent of palette indices. Runtime tests require a successful build.
"""

import hashlib
import json
from pathlib import Path
import random
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from tileset_lz77 import lz77_decompress
from dump_tiles import load_gameplay_assets

OUTPUT = ROOT / "build" / "assets"
MAPS = {
    "use_keys_8bit": (6, 4),
    "A_SAPPER_MAKES_ONLY_ONE_MISTAKE": (10, 1),
    "retro_divider_320x16": (20, 1),
    "soldier_neutral": (6, 8),
    "Congratulations": (14, 1),
    "Oops": (14, 1),
    "soldier_congratulations": (6, 8),
    "soldier_defeat": (6, 8),
}
# Rectangles in atlas tile coordinates: x, y, width, height.
RECTANGLES = {
    "A_SAPPER_MAKES_ONLY_ONE_MISTAKE": (0, 2, 10, 1),
    "Congratulations": (0, 4, 14, 1),
    "Oops": (0, 5, 14, 1),
    "use_keys_8bit": (10, 0, 6, 4),
    "soldier_neutral": (0, 0, 6, 8),
    "soldier_congratulations": (6, 0, 6, 8),
    "soldier_defeat": (12, 0, 6, 8),
}


ATLASES = {
    "board_tiles": (16, 6, 0),
    "divider_tiles": (6, 1, 96),
    "soldier_tiles": (18, 8, 102),
}


def atlas_for(name):
    if name == "retro_divider_320x16":
        return "divider_tiles"
    return "soldier_tiles" if name.startswith("soldier_") else "board_tiles"


def atlas_tiles(name):
    with Image.open(ROOT / "assets" / f"{name}.gif") as image:
        columns, rows = image.width // 16, image.height // 16
        return b"".join(
            image.crop(((t % columns) * 16, (t // columns) * 16,
                        (t % columns + 1) * 16, (t // columns + 1) * 16)).tobytes()
            for t in range(columns * rows)
        )


PALETTE_COUNTS = {"soldier_tiles": 41, "board_tiles": 14, "divider_tiles": 10}


def authored_palette():
    palette = bytearray()
    for name, count in PALETTE_COUNTS.items():
        with Image.open(ROOT / "assets" / f"{name}.gif") as image:
            rgb = image.getpalette()[:count * 3]
        palette.extend(b"".join(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
                                for r, g, b in zip(rgb[::3], rgb[1::3], rgb[2::3])))
    return bytes(palette)


def palette_offset(name):
    return sum(PALETTE_COUNTS[n] for n in list(PALETTE_COUNTS)[:list(PALETTE_COUNTS).index(name)])


def authored_tiles():
    return b"".join(bytes(index + palette_offset(name) if index else 0
                          for index in atlas_tiles(name)) for name in ATLASES)


def colored(raw):
    palette = authored_palette()
    return b"".join(bytes([index != 0]) + palette[index * 2:index * 2 + 2] for index in raw)



AUTHOR_TILE_COUNT = len(authored_tiles()) // 256


def digest(data):
    return hashlib.sha256(data).hexdigest()


def reconstruct(raw, cells, width):
    """Copy tile rows as render_image() does, yielding indexed image pixels."""
    return b"".join(
        raw[tile * 256 + y * 16:tile * 256 + (y + 1) * 16]
        for row in range(0, len(cells), width)
        for y in range(16)
        for tile in cells[row:row + width]
    )


class SourceAtlasTests(unittest.TestCase):
    def test_atlas_palette_hud_and_transparency(self):
        raw = authored_tiles()
        expected = json.loads((ROOT / "tests" / "board_game_tile_sha256.json").read_text())
        constants = {name: int(index) for name, index in re.findall(
            r"#define (TILE_\w+)\s+(\d+)", (ROOT / "src" / "zealmine.h").read_text())}
        for name, expected_hash in expected.items():
            index = constants[name]
            self.assertEqual(digest(colored(raw[index * 256:(index + 1) * 256])), expected_hash, name)
        for digit in range(10):
            self.assertEqual(constants[f"TILE_DIGIT{digit}"], 16 + digit)
        for name, (columns, rows, _) in ATLASES.items():
            with Image.open(ROOT / "assets" / f"{name}.gif") as image:
                self.assertEqual(image.mode, "P")
                self.assertEqual(image.size, (columns * 16, rows * 16))
                self.assertEqual(image.info.get("transparency"), 0)
                palette = image.getpalette()
                self.assertEqual(len(palette), (16 if name != "soldier_tiles" else 64) * 3)
                self.assertLess(max(image.tobytes()), PALETTE_COUNTS[name])
        blank, cursor = constants["TILE_BLANK"], constants["TILE_CURSOR"]
        self.assertEqual(raw[blank * 256:(blank + 1) * 256], bytes(256))
        self.assertIn(0, raw[cursor * 256:(cursor + 1) * 256])
        for name in expected:
            if name not in ("TILE_BLANK", "TILE_CURSOR"):
                index = constants[name]
                self.assertNotIn(0, raw[index * 256:(index + 1) * 256], name)

    def test_divider_has_six_unique_tiles(self):
        raw = atlas_tiles("divider_tiles")
        tiles = [raw[i:i + 256] for i in range(0, len(raw), 256)]
        self.assertEqual(len(tiles), 6)
        self.assertEqual(len(set(tiles)), 6)
        self.assertEqual(AUTHOR_TILE_COUNT, 246)

    def test_shared_tileset_and_maps(self):
        for name, (columns, rows, _) in ATLASES.items():
            tileset = ET.parse(ROOT / "assets" / f"{name}.tsx").getroot()
            for key, value in {"tilewidth": "16", "tileheight": "16",
                               "tilecount": str(columns * rows), "columns": str(columns)}.items():
                self.assertEqual(tileset.get(key), value)
            self.assertEqual(tileset.find("image").attrib,
                             {"source": f"{name}.gif", "width": str(columns * 16),
                              "height": str(rows * 16)})
        for name, (width, height) in MAPS.items():
            with self.subTest(map=name):
                root = ET.parse(ROOT / "assets" / f"{name}.tmx").getroot()
                for key, value in {"orientation": "orthogonal", "infinite": "0",
                                   "renderorder": "right-down", "tilewidth": "16",
                                   "tileheight": "16", "width": str(width),
                                   "height": str(height)}.items():
                    self.assertEqual(root.get(key), value)
                self.assertEqual(len(root.findall("tileset")), 1)
                atlas = atlas_for(name)
                columns, rows, offset = ATLASES[atlas]
                self.assertEqual(root.find("tileset").attrib,
                                 {"firstgid": str(offset + 1), "source": f"{atlas}.tsx"})
                layers = root.findall("layer")
                self.assertEqual(len(layers), 1)
                layer = layers[0]
                self.assertEqual(layer.get("id"), "1")
                self.assertEqual(layer.get("width"), str(width))
                self.assertEqual(layer.get("height"), str(height))
                data = layer.find("data")
                self.assertEqual(data.attrib, {"encoding": "csv"})
                gids = [int(n) for n in data.text.split(",")]
                self.assertEqual(len(gids), width * height)
                self.assertTrue(all(offset < n <= offset + columns * rows for n in gids))
                if name == "retro_divider_320x16":
                    expected = [97 + index for index in
                                [0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 1, 1, 5]]
                else:
                    x, y, w, h = RECTANGLES[name]
                    self.assertEqual((w, h), (width, height))
                    expected = [offset + (y + row) * columns + x + col + 1
                                for row in range(h) for col in range(w)]
                self.assertEqual(gids, expected)

    def test_all_picture_pixels(self):
        hashes = json.loads((ROOT / "tests" / "board_image_sha256.json").read_text())
        raw = authored_tiles()
        self.assertEqual(set(hashes), set(MAPS))
        for name, expected in hashes.items():
            with self.subTest(picture=name):
                root = ET.parse(ROOT / "assets" / f"{name}.tmx").getroot()
                cells = [int(n) - 1 for n in root.find("layer/data").text.split(",")]
                pixels = reconstruct(raw, cells, MAPS[name][0])
                self.assertEqual(digest(colored(pixels)), expected)
                if name == "retro_divider_320x16":
                    continue
                x, y, w, h = RECTANGLES[name]
                with Image.open(ROOT / "assets" / f"{atlas_for(name)}.gif") as image:
                    offset = palette_offset(atlas_for(name))
                    local = image.crop((x * 16, y * 16, (x + w) * 16, (y + h) * 16)).tobytes()
                    self.assertEqual(bytes(i + offset if i else 0 for i in local), pixels)


@unittest.skipIf(AUTHOR_TILE_COUNT > 256,
                 "authored atlas exceeds 256 tiles; runtime reduction deferred")
class BuiltAssetTests(unittest.TestCase):
    def test_final_asset_bytes(self):
        hashes = json.loads((ROOT / "tests" / "assets_sha256.json").read_text())
        self.assertEqual(len(hashes), 17)
        for name in hashes:
            with self.subTest(asset=name):
                self.assertEqual(digest((OUTPUT / name).read_bytes()), hashes[name])

    def test_sdk_tiles_and_maps(self):
        raw = authored_tiles()
        pixels, palette = load_gameplay_assets(OUTPUT)
        self.assertEqual(raw, pixels)
        self.assertEqual(authored_palette(), palette)
        for name in ATLASES:
            local = atlas_tiles(name)
            if name != "soldier_tiles":
                local = bytes((local[i] << 4) | local[i + 1] for i in range(0, len(local), 2))
            self.assertEqual(local, (OUTPUT / f"{name}.zts").read_bytes())
        for name in MAPS:
            root = ET.parse(ROOT / "assets" / f"{name}.tmx").getroot()
            cells = bytes(int(n) - 1 for n in root.find("layer/data").text.split(","))
            self.assertEqual(cells, (OUTPUT / f"{name}.ztm").read_bytes())

    def test_compression_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw.zts"
            output = Path(directory) / "packed.zts"
            source.write_bytes(authored_tiles())
            result = subprocess.run([sys.executable, str(ROOT / "tools" / "tileset_lz77.py"),
                                     "-i", str(source), "-o", str(output)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(lz77_decompress(output.read_bytes()), authored_tiles())


class CompressionValidationTests(unittest.TestCase):
    def test_concatenation_order_and_combined_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            first, second, output = [directory / name for name in ("first", "second", "output")]
            first.write_bytes(bytes([1]) * 256)
            second.write_bytes(bytes([2]) * 256)
            command = [sys.executable, str(ROOT / "tools" / "tileset_lz77.py"),
                       "-i", str(first), str(second), "-o", str(output)]
            subprocess.run(command, check=True)
            self.assertEqual(lz77_decompress(output.read_bytes()), first.read_bytes() + second.read_bytes())
            output.unlink()
            first.write_bytes(bytes(126 * 256))
            second.write_bytes(bytes(144 * 256))
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exceeds 256 tiles", result.stderr)
            self.assertFalse(output.exists())
            # Two partial inputs may add up to a tile but must still be rejected.
            first.write_bytes(bytes(128))
            second.write_bytes(bytes(128))
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("whole 256-byte tiles", result.stderr)
            self.assertFalse(output.exists())

    def test_compression_rejects_invalid_input(self):
        inputs = [(b"", "empty"), (bytes(255), "whole 256-byte tiles"),
                  (bytes(257 * 256), "exceeds 256 tiles"),
                  (random.Random(0).randbytes(256 * 256), "exceeds 65,535 bytes")]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw.zts"
            output = Path(directory) / "packed.zts"
            for data, message in inputs:
                with self.subTest(error=message):
                    source.write_bytes(data)
                    result = subprocess.run(
                        [sys.executable, str(ROOT / "tools" / "tileset_lz77.py"),
                         "-i", str(source), "-o", str(output)], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(message, result.stderr)
                    self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
