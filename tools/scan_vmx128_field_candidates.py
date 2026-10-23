#!/usr/bin/env python3
"""Find direct addi-to-lvx128 address idioms for selected object field offsets."""
import bisect
import json
import struct
from pathlib import Path


TEXT_BASE = 0x822B0000
LVX128_MASK = 0xFC0007F3
LVX128_VALUE = (4 << 26) | 195
FIELD_OFFSETS = {
    336, 352, 368, 384,
    480, 484, 488, 492, 496, 500, 504, 508,
    512, 516, 520, 524, 528, 532, 536, 540, 544, 548, 552, 556,
    560, 564, 568, 572, 576, 580, 584, 588, 592, 596, 600,
}


def word_at(data, address):
    return struct.unpack_from(">I", data, address - TEXT_BASE)[0]


def s16(value):
    return value - 0x10000 if value & 0x8000 else value


def main():
    root = Path(__file__).resolve().parents[1]
    text = (root / "tmp/scan/text.bin").read_bytes()
    index = json.loads((root / "tmp/scan/index/acv-xref.json").read_text())
    functions = sorted(index["functions"], key=lambda f: f["begin"])
    begins = [f["begin"] for f in functions]
    rows = []

    for offset in range(0, len(text) - 3, 4):
        address = TEXT_BASE + offset
        word = struct.unpack_from(">I", text, offset)[0]
        if word & LVX128_MASK != LVX128_VALUE:
            continue
        ra, rb = (word >> 16) & 31, (word >> 11) & 31
        target = ra if rb == 0 and ra != 0 else rb if ra == 0 and rb != 0 else None
        if target is None:
            continue
        for distance in range(1, 9):
            prior = address - 4 * distance
            if prior < TEXT_BASE:
                break
            insn = word_at(text, prior)
            if insn >> 26 != 14:  # addi
                continue
            rt, base = (insn >> 21) & 31, (insn >> 16) & 31
            disp = s16(insn & 0xFFFF)
            if rt != target or disp not in FIELD_OFFSETS:
                continue
            fi = bisect.bisect_right(begins, prior) - 1
            owner = None
            if fi >= 0 and prior < begins[fi] + functions[fi]["length"]:
                owner = functions[fi]
            rows.append({
                "addi_address": f"0x{prior:08X}",
                "lvx128_address": f"0x{address:08X}",
                "field_displacement": disp,
                "source_base_register": base,
                "temporary_register": target,
                "lvx_ra": ra,
                "lvx_rb": rb,
                "lvx_vector_register": ((word << 3) & 0x60) | ((word >> 21) & 31),
                "raw_addi": f"0x{insn:08X}",
                "raw_lvx128": f"0x{word:08X}",
                "owner": ({
                    "begin": f"0x{owner['begin']:08X}",
                    "length": owner["length"],
                } if owner else None),
                "distance_in_instructions": distance,
            })
            break

    print(json.dumps({
        "scope": "candidate index for addi(base, selected displacement) followed within eight instructions by lvx128 using that temporary and r0; source-base object identity is not established",
        "text_base": f"0x{TEXT_BASE:08X}",
        "field_offsets": sorted(FIELD_OFFSETS),
        "candidate_count": len(rows),
        "candidates": rows,
    }, indent=2))


if __name__ == "__main__":
    main()
