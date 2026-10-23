#!/usr/bin/env python3
"""List raw lvx128/stvx128 words in a guest-address window from a .text dump."""
import argparse
import json
import struct
from pathlib import Path


TEXT_BASE = 0x822B0000
MASK = 0xFC0007F3
OPCODES = {
    "lvx128": (4 << 26) | 195,
    "stvx128": (4 << 26) | 451,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("text_dump", type=Path)
    parser.add_argument("start", type=lambda x: int(x, 0))
    parser.add_argument("end", type=lambda x: int(x, 0), help="exclusive guest address")
    args = parser.parse_args()
    if args.start < TEXT_BASE or args.end < args.start:
        parser.error("invalid guest address range")
    data = args.text_dump.read_bytes()
    if args.end - TEXT_BASE > len(data):
        parser.error("range extends beyond the supplied .text dump")

    rows = []
    for address in range(args.start, args.end - 3, 4):
        word = struct.unpack_from(">I", data, address - TEXT_BASE)[0]
        for name, value in OPCODES.items():
            if word & MASK == value:
                rows.append({
                    "address": f"0x{address:08X}",
                    "word": f"0x{word:08X}",
                    "opcode": name,
                    "table_mask": f"0x{MASK:08X}",
                    "table_value": f"0x{value:08X}",
                    "vector_register": ((word << 3) & 0x60) | ((word >> 21) & 0x1F),
                    "ra": (word >> 16) & 0x1F,
                    "rb": (word >> 11) & 0x1F,
                })
                break
    print(json.dumps({
        "scope": "raw-word mask/value and register-field scan; pointer provenance and runtime effects excluded",
        "text_dump": str(args.text_dump),
        "text_base": f"0x{TEXT_BASE:08X}",
        "start": f"0x{args.start:08X}",
        "end_exclusive": f"0x{args.end:08X}",
        "matches": rows,
    }, indent=2))


if __name__ == "__main__":
    main()
