#!/usr/bin/env python3
"""Read-only report of direct callers of 0x82BC8980 and aligned generated annotations."""
import json
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT_BASE = 0x822B0000
TARGET = 0x82BC8980
TEXT = ROOT / "tmp/scan/text.bin"
PDATA = ROOT / "tmp/scan/pdata.bin"
GENERATED = ROOT / "tmp/xenon-out/ppc/ppc_recomp.496.cpp"


def disasm(start, end):
    p = subprocess.run(["python3", str(ROOT / "tools/ppc_dis.py"), str(TEXT),
                        hex(start), hex(end)], check=True, capture_output=True, text=True)
    return p.stdout.splitlines()


def main():
    text = TEXT.read_bytes()
    words = struct.unpack(">%dI" % (len(text) // 4), text[:len(text) // 4 * 4])
    pd = PDATA.read_bytes()
    funcs = sorted((b, ((w >> 8) & 0x3FFFFF) * 4) for b, w in
                   (struct.unpack(">II", pd[i:i + 8]) for i in range(0, len(pd) - 7, 8)))

    def owner(addr):
        for start, size in funcs:
            if start <= addr < start + size:
                return start, size
        return None

    calls = []
    for i, word in enumerate(words):
        addr = TEXT_BASE + 4 * i
        if word >> 26 == 18 and word & 1:
            disp = word & 0x03FFFFFC
            if disp & 0x02000000:
                disp -= 0x04000000
            if (addr + disp) & 0xFFFFFFFF == TARGET:
                calls.append((addr, owner(addr)))

    print("DIRECT_CALLERS", len(calls))
    for call, fn in calls:
        print(f"\n=== call 0x{call:08X} owner=" + (f"[0x{fn[0]:08X},0x{fn[0]+fn[1]:08X})" if fn else "unknown") + " ===")
        if not fn:
            print("no .pdata owner")
            continue
        lines = disasm(fn[0], fn[0] + fn[1])
        idx = next((i for i, line in enumerate(lines) if int(line[:8], 16) == call), None)
        lo, hi = max(0, (idx or 0) - 30), min(len(lines), (idx or 0) + 5)
        print("\n".join(lines[lo:hi]))

    generated = GENERATED.read_text(errors="replace").splitlines()
    begin = next(i for i, line in enumerate(generated)
                 if line.startswith("PPC_FUNC_IMPL(__imp__sub_82BC8980)"))
    end = next((i for i in range(begin + 1, len(generated))
                if generated[i].startswith("PPC_FUNC_IMPL(")), len(generated))
    annotations = []
    for line in generated[begin + 1:end]:
        m = re.match(r"\s*//\s+([a-z][a-z0-9.]*)\s*(.*)$", line)
        if m:
            annotations.append((m.group(1), m.group(2).strip()))
    print("\n=== helper aligned instruction annotations ===")
    for i, (mnemonic, operands) in enumerate(annotations):
        print(f"0x{TARGET + i*4:08X} {mnemonic:16s} {operands}")


if __name__ == "__main__":
    main()
