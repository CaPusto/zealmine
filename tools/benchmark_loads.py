#!/usr/bin/env python3
"""Measure uninstrumented asset loaders in zeal-native Z80 T-states."""

import argparse
import json
import os
from pathlib import Path
import re
import selectors
import subprocess
import tempfile
import time


def measure(emulator, rom, binary, link_map):
    symbols = {name: int(address, 16) for address, name in re.findall(
        r"^\s+([0-9A-Fa-f]{8})\s+(_\w+)\s", Path(link_map).read_text(), re.M)}
    with tempfile.TemporaryDirectory() as hostfs:
        child = subprocess.Popen([emulator, "--console", "--debug", "--rom", rom,
                                  "-u", binary, "-H", hostfs], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector = selectors.DefaultSelector()
        selector.register(child.stdout, selectors.EVENT_READ)

        def output():
            result = bytearray()
            deadline = time.monotonic() + 30
            while not result.endswith(b"debug> "):
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not selector.select(remaining):
                    raise TimeoutError(result.decode(errors="replace"))
                data = os.read(child.stdout.fileno(), 65536)
                if not data:
                    raise RuntimeError(result.decode(errors="replace"))
                result.extend(data)
            return result.decode(errors="replace")

        def command(text):
            child.stdin.write((text + "\n").encode())
            child.stdin.flush()
            return output()

        def cycles(text):
            return int(re.search(r"Paused @ 0x[0-9a-f]+ \(cyc=(\d+)\)", text)[1])

        def finish_load(name):
            address = symbols[name]
            start = cycles(command("continue"))
            registers = command("regs")
            assert int(re.search(r"PC=([0-9a-f]+)", registers)[1], 16) == address, registers
            sp = int(re.search(r"SP=([0-9a-f]+)", registers)[1], 16)
            ret = int(re.search(r"0x[0-9a-f]+: ([0-9a-f]+)", command(f"r16 {sp}"))[1], 16)
            command(f"bc {address}")
            command(f"bp {ret}")
            end = cycles(command("continue"))
            registers = command("regs")
            assert int(re.search(r"AF=([0-9a-f]+)", registers)[1], 16) >> 8 == 0, registers
            command(f"bc {ret}")
            return end - start

        try:
            output()
            command(f"bp {symbols['_title_assets_load']}")
            title = finish_load("_title_assets_load")
            command(f"bp {symbols['_assets_load']}")
            command("run 2000000")
            command("press SPACE")
            gameplay = finish_load("_assets_load")
            return {"title_tstates": title, "gameplay_tstates": gameplay}
        finally:
            selector.close()
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
            child.stdin.close()
            child.stdout.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", default="zeal-native")
    parser.add_argument("--rom", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--map", required=True)
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    results = [measure(args.emulator, args.rom, args.binary, args.map)
               for _ in range(args.runs)]
    print(json.dumps({"binary_bytes": Path(args.binary).stat().st_size,
                      "runs": results}, indent=2))


if __name__ == "__main__":
    main()
