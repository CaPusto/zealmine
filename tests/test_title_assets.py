"""Title packing and native loader regression tests; no working board build needed."""

import hashlib
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from pack_title import pack_title
from tileset_lz77 import lz77_compress, lz77_decompress

# Decoded title VRAM before the 4bpp storage change, including blank tile 0.
TITLE_VRAM_SHA256 = "d6994367977ec57833ef0616688fd3dc427426a7687687c513b8180f1f31346e"


class TitleTests(unittest.TestCase):
    def test_byte_decoder_independent_of_graphics(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            harness = directory / "decoder"
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "src"), str(ROOT / "tests" / "test_lz77.c"),
                            str(ROOT / "src" / "tileset_lz77.c"), "-o", str(harness)], check=True)
            rng = random.Random(9)
            fixtures = [rng.randbytes(n) for n in [0, 1, 255, 256, 257, 4097]]
            fixtures += [b"ABC" * 1400, bytes(range(256)) * 3 + b"end", bytes(1025)]
            for raw in fixtures:
                stream, expected = directory / "stream", directory / "expected"
                stream.write_bytes(lz77_compress(raw)); expected.write_bytes(raw)
                subprocess.run([str(harness), str(stream), str(expected)], check=True)

    def test_title_source_fits_4bit(self):
        with Image.open(ROOT / "assets" / "zeal_mine_title.gif") as image:
            self.assertEqual(image.size, (320, 240))
            self.assertEqual(len(image.getpalette()), 16 * 3)
            self.assertTrue(all(1 <= index <= 15 for index in image.tobytes()))

    def test_packing_and_native_upload(self):
        # Reproduce raw SDK nibble packing and first-occurrence tile ordering.
        with Image.open(ROOT / "assets" / "zeal_mine_title.gif") as image:
            tiles = [image.crop((x * 16, y * 16, (x + 1) * 16, (y + 1) * 16)).tobytes()
                     for y in range(15) for x in range(20)]
        unique = list(dict.fromkeys(tiles))
        pixels = bytes(256) + b"".join(unique)
        self.assertEqual(hashlib.sha256(pixels).hexdigest(), TITLE_VRAM_SHA256)
        raw = bytes((pixels[i] << 4) | pixels[i + 1] for i in range(0, len(pixels), 2))
        tilemap = bytes(unique.index(tile) for tile in tiles)
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, output = directory / "sdk", directory / "title"
            source.with_suffix(".zts").write_bytes(raw[128:])
            # Palette colors do not affect packing; valid-sized fixture suffices.
            source.with_suffix(".ztp").write_bytes(bytes(32))
            source.with_suffix(".ztm").write_bytes(tilemap)
            pack_title(source, output)
            compressed = output.with_suffix(".zts").read_bytes()
            self.assertEqual(lz77_decompress(compressed), raw)
            self.assertEqual(output.with_suffix(".ztm").read_bytes(), bytes(i + 1 for i in tilemap))
            self.assertEqual(output.with_suffix(".ztp").read_bytes(), bytes(32))
            harness = directory / "loader"
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "tests" / "stubs"), "-I" + str(ROOT / "src"),
                            str(ROOT / "tests" / "test_tileset_lz77.c"),
                            str(ROOT / "src" / "tileset_lz77.c"),
                            str(ROOT / "src" / "tileset_loader.c"), "-o", str(harness)], check=True)
            expected = directory / "pixels.raw"
            expected.write_bytes(raw)
            # Odd tile count tests the final half-window; packed native VRAM pages
            # and overlapping/cross-tile back-references use real title data.
            subprocess.run([str(harness), "4", str(output.with_suffix(".zts")), str(expected)], check=True)
            board = directory / "board.zts"
            board.write_bytes(lz77_compress(pixels))
            expected.write_bytes(pixels)
            subprocess.run([str(harness), "8", str(board), str(expected)], check=True)
            # Include zero nibbles within a tile and an even tile count.
            random_pixels = bytes(random.Random(0).choices(range(16), k=4 * 256))
            packed = bytes((random_pixels[i] << 4) | random_pixels[i + 1]
                           for i in range(0, len(random_pixels), 2))
            expected.write_bytes(packed)
            output.with_suffix(".zts").write_bytes(lz77_compress(packed))
            subprocess.run([str(harness), "4", str(output.with_suffix(".zts")), str(expected)], check=True)

    def test_gameplay_expansion_and_palette_offsets(self):
        # Real SDK output exercises all three streams and transparent zero.
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            harness = directory / "loader"
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "tests" / "stubs"), "-I" + str(ROOT / "src"),
                            str(ROOT / "tests" / "test_tileset_lz77.c"),
                            str(ROOT / "src" / "tileset_lz77.c"),
                            str(ROOT / "src" / "tileset_loader.c"), "-o", str(harness)], check=True)
            for name, offset, mode in [("board", 41, "expand"), ("divider", 55, "expand"),
                                       ("soldier", 0, "8")]:
                stream = ROOT / "build" / "assets" / f"{name}.zts"
                raw = lz77_decompress(stream.read_bytes())
                if mode == "expand":
                    raw = bytes(n for byte in raw for n in (byte >> 4, byte & 15))
                expected = directory / "expected"
                expected.write_bytes(bytes(index + offset if index else 0 for index in raw))
                subprocess.run([str(harness), mode, str(stream), str(expected), str(offset), "1"], check=True)
            # Odd tile count with zero pixels and nonzero offset tests half-window.
            raw = bytes(random.Random(7).choices(range(16), k=3 * 256))
            packed = bytes((raw[i] << 4) | raw[i + 1] for i in range(0, len(raw), 2))
            stream = directory / "odd.zts"
            stream.write_bytes(lz77_compress(packed))
            expected.write_bytes(bytes(index + 14 if index else 0 for index in raw))
            subprocess.run([str(harness), "expand", str(stream), str(expected), "14", "1"], check=True)

    def test_reject_transparent_nibbles_and_invalid_sizes(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "sdk", Path(directory) / "title"
            source.with_suffix(".ztm").write_bytes(bytes(300))
            source.with_suffix(".ztp").write_bytes(bytes(32))
            for value in [0x01, 0x10]:
                source.with_suffix(".zts").write_bytes(bytes([value]) * 128)
                with self.assertRaisesRegex(ValueError, "reserve palette index 0"):
                    pack_title(source, output)
            source.with_suffix(".zts").write_bytes(bytes([0x11]) * 127)
            with self.assertRaisesRegex(ValueError, "whole raw 4bpp tiles"):
                pack_title(source, output)
            source.with_suffix(".zts").write_bytes(bytes([0x11]) * 128)
            source.with_suffix(".ztp").write_bytes(bytes(512))
            with self.assertRaisesRegex(ValueError, "16-entry"):
                pack_title(source, output)


if __name__ == "__main__":
    unittest.main()
