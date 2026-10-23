#!/usr/bin/env python3
"""Read-only callsite windows for direct callers of retail function 0x82302520."""
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT_BASE = 0x822B0000
TARGET = 0x82302520
TEXT = ROOT / "tmp/scan/text.bin"
PDATA = ROOT / "tmp/scan/pdata.bin"


def main():
    raw = TEXT.read_bytes()
    words = struct.unpack(">%dI" % (len(raw) // 4), raw[:len(raw) // 4 * 4])
    pdata = PDATA.read_bytes()
    functions = sorted((begin, ((word >> 8) & 0x3FFFFF) * 4) for begin, word in
                       (struct.unpack(">II", pdata[i:i + 8])
                        for i in range(0, len(pdata) - 7, 8)))

    def owner(address):
        return next(((begin, size) for begin, size in functions
                     if begin <= address < begin + size), None)

    calls = []
    for i, word in enumerate(words):
        if word >> 26 != 18 or not word & 1:
            continue
        address = TEXT_BASE + i * 4
        displacement = word & 0x03FFFFFC
        if displacement & 0x02000000:
            displacement -= 0x04000000
        if (address + displacement) & 0xFFFFFFFF == TARGET:
            calls.append((address, owner(address)))

    print(f"DIRECT_CALL_COUNT {len(calls)}")
    for address, fn in calls:
        print(f"\n=== caller=0x{address:08X} owner=" +
              (f"[0x{fn[0]:08X},0x{fn[0]+fn[1]:08X})" if fn else "unknown") + " ===")
        start = fn[0] if fn else max(TEXT_BASE, address - 0x60)
        end = address + 4
        decoded = subprocess.run(
            ["python3", str(ROOT / "tools/ppc_dis.py"), str(TEXT), hex(start), hex(end)],
            check=True, capture_output=True, text=True).stdout.splitlines()
        print("\n".join(decoded))


if __name__ == "__main__":
    main()
