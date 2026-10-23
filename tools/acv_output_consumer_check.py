#!/usr/bin/env python3
"""Check the raw VMX128 load that consumes object+208 in 0x82301958."""
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT_BASE = 0x822B0000
ADDRESSES = (0x82301A10, 0x82301A20, 0x82301A30, 0x82301A4C)
EXPECTED_RB = (7, 8, 11, 9)
TEXT = ROOT / "tmp/scan/text.bin"
SOURCE = ROOT / "tools/XenonRecomp/thirdparty/disasm/ppc-dis.c"
GENERATED = ROOT / "tmp/xenon-out/ppc/ppc_recomp.17.cpp"
MASK = (0x3F << 26) | 0x7F3
VALUE = (4 << 26) | (195 & 0x7F3)  # VX128_1(4, 195): lvx128


def main():
    data = TEXT.read_bytes()
    rows = SOURCE.read_text().splitlines()
    table_row = next(line.strip() for line in rows if '"lvx128"' in line)
    generated = GENERATED.read_text(errors="replace").splitlines()
    impl = next(i for i, line in enumerate(generated)
                if line.startswith("PPC_FUNC_IMPL(__imp__sub_82301958)"))
    end = next((i for i in range(impl + 1, len(generated))
                if generated[i].startswith("PPC_FUNC_IMPL(")), len(generated))
    annotations = []
    for line in generated[impl + 1:end]:
        match = re.match(r"\s*//\s+([a-z][a-z0-9.]*)\s*(.*)$", line)
        if match:
            annotations.append((match.group(1), match.group(2).strip()))
    loads = []
    for address, expected_rb in zip(ADDRESSES, EXPECTED_RB):
        offset = address - TEXT_BASE
        word = struct.unpack_from(">I", data, offset)[0]
        rd, ra, rb = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
        ordinal = (address - 0x82301958) // 4
        mnemonic, operands = annotations[ordinal]
        loads.append({
            "address": f"0x{address:08X}",
            "raw_word": f"0x{word:08X}",
            "raw_word_matches_pinned_VX128_1_mask": (word & MASK) == VALUE,
            "RA_field": ra,
            "RB_field": rb,
            "expected_RB_from_preceding_li": expected_rb,
            "generated_annotation_at_same_address": f"{mnemonic} {operands}".strip(),
            "generated_annotation_matches_raw_opcode_and_RA_RB": mnemonic == "lvx128" and f"r{ra},r{rb}" in operands,
            "guest_effective_address_before_alignment": "r11+r" + str(rb) if ra == 11 else "r" + str(rb),
        })
    result = {
        "text_sha256": hashlib.sha256(data).hexdigest(),
        "opcode": "lvx128",
        "pinned_table_row": table_row,
        "loads": loads,
        "generated_source": str(GENERATED.relative_to(ROOT)),
        "raw_address_setup": ["0x82301964: li r7,16", "0x82301984: li r8,32", "0x8230198C: li r9,48", "0x8230197C: addi r11,r3,208"],
        "interpretation_scope": "four raw VMX128 loads in function 0x82301958 address object+224,+240,+208,+256; confirms all four 16-byte slots in the 64-byte output region are read, not their higher-level type or purpose",
    }
    print(json.dumps(result, indent=2))
    if any(not load["raw_word_matches_pinned_VX128_1_mask"] or
           load["RB_field"] != load["expected_RB_from_preceding_li"] or
           not load["generated_annotation_matches_raw_opcode_and_RA_RB"] for load in loads):
        raise SystemExit("VMX128 load check failed")


if __name__ == "__main__":
    main()
