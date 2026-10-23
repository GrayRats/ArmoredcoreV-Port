#!/usr/bin/env python3
"""Audit one isolated cell-dialect change against the ACV raw-word report."""
import hashlib
import json
from pathlib import Path
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "logs/decomp.sqlite3"
DECODE = ROOT / "logs/acv-dialect-audit-20261020/xenia-decode-audit.json"
SOURCE = ROOT / "tmp/xenon-more-cell-source/thirdparty/disasm/ppc-dis.c"
BUILD = ROOT / "tmp/xenon-more-cell-build/XenonRecomp/XenonRecomp"
OUTPUT_DIR = ROOT / "tmp/xenon-more-cell-output"
FUNCTION = OUTPUT_DIR / "ppc_recomp.592.cpp"
REPORT = ROOT / "logs/acv-dialect-audit-20261020/cell-dialect-coverage.json"
TARGET = "0x82f12b9c"
OWNER = "0x82f12790"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    con = sqlite3.connect(DB)
    operation = con.execute(
        "SELECT id, exit_code, stdout, stderr FROM operations "
        "WHERE command_json LIKE '%acv-more-cell.toml%' "
        "ORDER BY started_at DESC LIMIT 1").fetchone()
    if not operation or operation[1] != 0:
        raise RuntimeError("No successful isolated generator process in SQLite")
    op_id, exit_code, stdout, stderr = operation
    output = (stdout or "") + "\n" + (stderr or "")
    unrecognized = {m.group(1).lower() for m in re.finditer(
        r"Unrecognized instruction at (0x[0-9A-Fa-f]+):", output)}
    if len(unrecognized) != 34 or TARGET in unrecognized:
        raise RuntimeError(f"Unexpected coverage report: {len(unrecognized)} / target={TARGET in unrecognized}")

    decode = json.loads(DECODE.read_text())
    row = next(r for r in decode["classifications"] if r["address"].lower() == TARGET)
    owner = next(o for o in row["pdata_owners"] if o["start"].lower() == OWNER)
    offset = int(TARGET, 16) - int(OWNER, 16)
    instruction_index = offset // 4
    lines = FUNCTION.read_text(errors="replace").splitlines()
    start = next(i for i, line in enumerate(lines)
                 if "PPC_FUNC_IMPL(__imp__sub_82F12790)" in line)
    comments = []
    target_comment = None
    for line in lines[start + 1:]:
        if line.startswith("PPC_FUNC_IMPL("):
            break
        if line.lstrip().startswith("// "):
            comments.append(line.strip())
            if len(comments) - 1 == instruction_index:
                target_comment = line.strip()
                break
    if target_comment != "// vsldoi128 v30,v24,v57,12":
        raise RuntimeError(f"Unexpected generated instruction at target ordinal: {target_comment}")
    expression = lines[lines.index(next(x for x in lines if target_comment in x)) + 1].strip()
    if "_mm_alignr_epi8" not in expression or ", 4));" not in expression:
        raise RuntimeError(f"Expected SHB=12 alignr expression, got {expression}")

    report = {
        "scope": "isolated cell-dialect classification and generation coverage only; no runtime proof",
        "change": "when option cell is enabled, clear PPC_OPCODE_403 before adding Cell/VMX128 flags",
        "source_path": str(SOURCE.relative_to(ROOT)),
        "source_sha256": sha256(SOURCE),
        "generator_binary_sha256": sha256(BUILD),
        "generator_sqlite_operation_id": op_id,
        "underlying_process_exit_code": exit_code,
        "wrapper_status_note": "warning-bearing stdout triggers the SQLite wrapper failure heuristic",
        "raw_word": row["raw_word"], "target_address": TARGET,
        "pdata_owner": OWNER, "function_offset": f"0x{offset:X}",
        "instruction_ordinal_from_owner_start": instruction_index,
        "generated_instruction_comment": target_comment,
        "generated_host_expression": expression,
        "remaining_unrecognized_addresses": len(unrecognized),
        "target_still_unrecognized": TARGET in unrecognized,
        "generated_cpp_files": len(list(OUTPUT_DIR.glob("*.cpp"))),
        "generated_function_path": str(FUNCTION.relative_to(ROOT)),
        "generated_function_sha256": sha256(FUNCTION),
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"remaining_unrecognized": len(unrecognized),
                      "target": TARGET, "decoded_as": "vsldoi128",
                      "generated_expression": True}))
    print(f"Evidence: {REPORT}")


if __name__ == "__main__":
    main()
