#!/usr/bin/env python3
"""Read-only monitor of the ACV step consumer accumulator flag.

Reads Xenia's stdout log for `log_lr_at_instruction` hits on 0x82460848
(consumer dispatcher entry, r3 = S = this+36), takes the distinct S values
seen recently, and polls their accumulator fields in guest memory:
  S+0 flag (byte), S+1 mode (byte), S+4 period (float), S+8 accumulator (float).
Prints a line whenever a field changes, so a 0 -> 1 flag transition can be
matched to what is on screen at that moment.

usage: monitor_consumer_flags.py LOG PID [--interval SEC] [--seconds N]
"""
import argparse
import re
import struct
import time

GUEST_OFFSET = 0x40000000
HOST_BASE = 0x140000000
HIT = re.compile(r"lr=82460A58 r3=([0-9A-F]{8})")


def read_guest(pid, guest, n):
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as mem:
        mem.seek(HOST_BASE + (guest - GUEST_OFFSET))
        return mem.read(n)


def recent_s_values(log_path, tail_bytes=2_000_000):
    with open(log_path, "rb") as fh:
        fh.seek(0, 2)
        size = fh.tell()
        fh.seek(max(0, size - tail_bytes))
        text = fh.read().decode("utf-8", errors="replace")
    return sorted({int(m.group(1), 16) for m in HIT.finditer(text)})


def snapshot(pid, s):
    flag = read_guest(pid, s, 1)[0]
    mode = read_guest(pid, s + 1, 1)[0]
    period = struct.unpack(">f", read_guest(pid, s + 4, 4))[0]
    acc = struct.unpack(">f", read_guest(pid, s + 8, 4))[0]
    return flag, mode, period, acc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("pid", type=int)
    ap.add_argument("--interval", type=float, default=0.25)
    ap.add_argument("--seconds", type=float, default=600)
    args = ap.parse_args()

    s_values = recent_s_values(args.log)
    print(f"tracking {len(s_values)} S values: " + " ".join(f"{s:08X}" for s in s_values), flush=True)
    last = {}
    end = time.time() + args.seconds
    while time.time() < end:
        for s in s_values:
            try:
                cur = snapshot(args.pid, s)
            except OSError as exc:
                print(f"{time.strftime('%H:%M:%S')} S={s:08X} read error: {exc}", flush=True)
                continue
            if last.get(s) != cur:
                flag, mode, period, acc = cur
                print(f"{time.strftime('%H:%M:%S')} S={s:08X} flag={flag} mode={mode} "
                      f"period={period:.4f} acc={acc:.4f}", flush=True)
                last[s] = cur
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
