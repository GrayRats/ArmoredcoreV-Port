#!/usr/bin/env python3
"""Align XenonRecomp's emitted PPC comments for 0x82BC8980 with retail words."""
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT_BASE = 0x822B0000
START = 0x82BC8980
SIZE = 0x240
TEXT = ROOT / "tmp/scan/text.bin"
GENERATED = ROOT / "tmp/xenon-out/ppc/ppc_recomp.496.cpp"
VECTOR_MNEMONIC = re.compile(r"^(?:lvs[lr]|(?:lv|st)[a-z0-9]+128|v[a-z0-9]+(?:128)?)$")
OPCODE_ROW = re.compile(
    r'^\s*\{\s*"([^"]+)"\s*,\s*([A-Z0-9_]+)\(([^)]*)\)\s*,\s*([A-Z0-9_]+_MASK)')
MASKS = {
    "VX_MASK": 0x7FF,
    "VX128_MASK": 0x3D0,
    "VX128_1_MASK": 0x7F3,
    "VX128_2_MASK": 0x210,
    "VX128_3_MASK": 0x7F0,
    "VX128_P_MASK": 0x630,
    "VX128_4_MASK": 0x730,
    "VX128_5_MASK": 0x10,
    "X_MASK": (0x3FF << 1) | 1,
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    generated = GENERATED.read_bytes()
    lines = generated.decode("utf-8", errors="replace").splitlines()
    begin = next(i for i, line in enumerate(lines)
                if line.startswith("PPC_FUNC_IMPL(__imp__sub_82BC8980)"))
    end = next((i for i in range(begin + 1, len(lines))
                if lines[i].startswith("PPC_FUNC_IMPL(")), len(lines))
    body = lines[begin + 1:end]

    # Generated translation emits one guest-instruction comment for each input word.
    annotations = []
    for line in body:
        match = re.match(r"\s*//\s+([a-z][a-z0-9.]*)\s*(.*)$", line)
        if match:
            annotations.append((match.group(1), match.group(2).strip()))

    if len(annotations) != SIZE // 4:
        raise SystemExit(f"Expected {SIZE // 4} instruction annotations, got {len(annotations)}")

    text = TEXT.read_bytes()
    raw = text[START - TEXT_BASE:START - TEXT_BASE + SIZE]
    if len(raw) != SIZE:
        raise SystemExit(f"Text dump truncated: got {len(raw)} bytes")

    source = (ROOT / "tools/XenonRecomp/thirdparty/disasm/ppc-dis.c").read_text()
    opcode_rows = {}
    for line in source.splitlines():
        match = OPCODE_ROW.match(line)
        if match:
            name, form, args, mask_name = match.groups()
            if mask_name not in MASKS or form not in {
                "VX", "VX128", "VX128_1", "VX128_2", "VX128_3",
                "VX128_P", "VX128_4", "VX128_5", "X",
            }:
                continue
            values = [int(x.strip(), 0) for x in args.split(",")]
            if len(values) != 2:
                continue
            op, xop = values
            if form in {"VX", "VX128"}:
                value = (op << 26) | (xop & MASKS[mask_name])
                mask = (0x3F << 26) | MASKS[mask_name]
            elif form.startswith("VX128_"):
                value = (op << 26) | (xop & MASKS[mask_name])
                mask = (0x3F << 26) | MASKS[mask_name]
            elif form == "X":
                value = (op << 26) | ((xop & 0x3FF) << 1)
                mask = (0x3F << 26) | MASKS[mask_name]
            else:
                continue
            opcode_rows[name] = (value, mask, form)

    rows = []
    for ordinal, (mnemonic, operands) in enumerate(annotations):
        address = START + ordinal * 4
        word = struct.unpack_from(">I", raw, ordinal * 4)[0]
        if VECTOR_MNEMONIC.match(mnemonic):
            table = opcode_rows.get(mnemonic)
            rows.append({
                "address": f"0x{address:08X}",
                "raw_word": f"{word:08X}",
                "generated_decoder_annotation": mnemonic + (" " + operands if operands else ""),
                "primary_opcode": word >> 26,
                "decoder_source_table_match": bool(table and (word & table[1]) == table[0]),
                "decoder_table_form": table[2] if table else None,
            })

    print(json.dumps({
        "scope": "align raw words to XenonRecomp-generated comments and match vector opcode masks/values against the pinned disassembly table; helper operands remain generated annotations, no runtime proof",
        "start": f"0x{START:08X}",
        "end_exclusive": f"0x{START + SIZE:08X}",
        "instruction_count": len(annotations),
        "text_sha256": sha256(text),
        "generated_source": str(GENERATED.relative_to(ROOT)),
        "generated_source_sha256": sha256(generated),
        "generated_vector_comment_count": len(rows),
        "vector_words_matching_source_table": sum(r["decoder_source_table_match"] for r in rows),
        "vector_words_without_matching_source_table": [r["address"] for r in rows if not r["decoder_source_table_match"]],
        "vector_annotations": rows,
    }, indent=2))


if __name__ == "__main__":
    main()
