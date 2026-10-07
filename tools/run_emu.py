#!/usr/bin/env python3
"""Drives zeal-native on a pty and drives the game with key taps.

ZOS sends its console to the video layer, so the program's output is only
visible when the emulator runs on a terminal. The host filesystem is mounted
as H:, which is where the DEBUG build writes zm.dbg.
"""

import argparse
import shutil
import tempfile
import os
import sys

import pexpect

Z80_HZ = 10_000_000


def tstates(ms):
    return ms * Z80_HZ // 1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--emulator", required=True)
    ap.add_argument("--hostfs", default=None)
    ap.add_argument(
        "--script",
        default="run 40000000",
        help="semicolon separated emulator commands, or key taps: 'tap SPACE'",
    )
    ap.add_argument("--timeout", type=int, default=20)
    ap.add_argument("--log", default=None)
    args = ap.parse_args()

    if args.hostfs is None:
        hostfs = tempfile.mkdtemp(prefix="zealmine_hostfs_")
    else:
        hostfs = args.hostfs
        if os.path.isdir(hostfs) and not os.listdir(hostfs):
            pass
        else:
            hostfs = tempfile.mkdtemp(prefix="zealmine_hostfs_")
            for name in os.listdir(args.hostfs):
                shutil.copy(os.path.join(args.hostfs, name), hostfs)

    argv = [
        args.emulator,
        "--headless",
        "--console",
        "--debug",
        "--rom",
        args.rom,
        "-u",
        args.binary,
    ]
    argv += ["-H", hostfs]

    logfile = None
    if args.log:
        logfile = open(args.log, "w", encoding="utf-8", errors="replace")

    child = pexpect.spawn(
        argv[0], argv[1:], encoding="utf-8", timeout=args.timeout, logfile=logfile
    )
    transcript = []

    def drain(seconds):
        try:
            data = child.read_nonblocking(size=65536, timeout=seconds)
        except (pexpect.TIMEOUT, pexpect.EOF):
            return
        transcript.append(data)

    try:
        child.expect_exact("debug> ", timeout=args.timeout)
        child.sendline("bp 0x4000")
        child.expect_exact("Breakpoint set", timeout=args.timeout)
        child.sendline("continue")
        child.expect(r"Paused @ 0x4000", timeout=args.timeout)
        print("entry point reached at 0x4000", flush=True)

        for step in args.script.split(";"):
            step = step.strip()
            if not step:
                continue
            child.sendline(step)
            if step.startswith(("tap ", "press ")):
                drain(0.3)
                if step.startswith("tap "):
                    child.sendline("release " + step.split()[1])
            elif step == "run" or step.startswith("run "):
                drain(1.0)
            else:
                drain(0.3)
            print("ran: %s" % step, flush=True)

        child.sendline("quit")
        drain(3.0)
    except pexpect.TIMEOUT:
        print("timeout while driving the emulator", flush=True)
    except pexpect.EOF:
        print("emulator exited", flush=True)
    finally:
        if child.isalive():
            child.terminate(force=True)
        if logfile:
            logfile.close()

    if args.hostfs is None:
        print("hostfs:", hostfs, flush=True)
    out = "".join(transcript)
    if out.strip():
        print("--- emulator output ---")
        print(out)


if __name__ == "__main__":
    main()
