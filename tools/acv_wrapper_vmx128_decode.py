#!/usr/bin/env python3
"""Check the 8 VMX128 load/store opcode words in 0x82300948 against XenonRecomp's table."""
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT = ROOT / "tmp/scan/text.bin"
SOURCE = ROOT / "tools/XenonRecomp/thirdparty/disasm/ppc-dis.c"
TEXT_BASE = 0x822B0000
ROWS = [
    (0x82300990, "lvx128", 195),
    (0x82300998, "lvx128", 195),
    (0x8230099C, "lvx128", 195),
    (0x823009A0, "lvx128", 195),
    (0x823009A4, "stvx128", 451),
    (0x823009A8, "stvx128", 451),
    (0x823009AC, "stvx128", 451),
    (0x823009B0, "stvx128", 451),
]


def main():
    data = TEXT.read_bytes()
    src = SOURCE.read_bytes()
    mask = (0x3F << 26) | 0x7F3  # VX128_1_MASK: top six opcode bits + xop mask.
    source_text = src.decode("utf-8")
    required = [
        '#define VX128_1(op, xop) (OP(op) | (((unsigned long)(xop)) & 0x7f3))',
        '#define VX128_1_MASK\tVX(0x3f, 0x7f3)',
        '{ "lvx128", VX128_1(4, 195), VX128_1_MASK',
        '{ "stvx128", VX128_1(4, 451), VX128_1_MASK',
        'return ((insn << 3) & 0x60) | ((insn >> 21) & 0x1f);',
    ]
    if not all(x in source_text for x in required):
        raise SystemExit("Pinned decoder source does not contain expected VX128_1 definitions")

    result = []
    for address, name, xop in ROWS:
        offset = address - TEXT_BASE
        raw = struct.unpack_from(">I", data, offset)[0]
        expected = (4 << 26) | xop
        result.append({
            "address": f"0x{address:08X}",
            "raw_word": f"{raw:08X}",
            "decoded_opcode": name,
            "mask": f"0x{mask:08X}",
            "masked_word": f"0x{raw & mask:08X}",
            "opcode_value": f"0x{expected:08X}",
            "matches_decoder_table": (raw & mask) == expected,
            "decoded_vector_register": ((raw << 3) & 0x60) | ((raw >> 21) & 0x1F),
            "decoded_ra": (raw >> 16) & 0x1F,
            "decoded_rb": (raw >> 11) & 0x1F,
        })
    print(json.dumps({
        "scope": "static raw-word / pinned decoder-table encoding and register-field check; effective-address and runtime semantics excluded",
        "text_sha256": hashlib.sha256(data).hexdigest(),
        "decoder_source": str(SOURCE.relative_to(ROOT)),
        "decoder_source_sha256": hashlib.sha256(src).hexdigest(),
        "results": result,
        "all_match": all(x["matches_decoder_table"] for x in result),
    }, indent=2))


if __name__ == "__main__":
    main()
