#!/usr/bin/env python3
"""Check isolated lhbrx handler coverage against generator diagnostics."""
import hashlib
import json
from pathlib import Path
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "logs/decomp.sqlite3"
DECODE = ROOT / "logs/acv-dialect-audit-20261020/xenia-decode-audit.json"
SOURCE = ROOT / "tmp/xenon-more-lhbrx-source/XenonRecomp/recompiler.cpp"
BUILD = ROOT / "tmp/xenon-more-lhbrx-build/XenonRecomp/XenonRecomp"
OUTPUT_DIR = ROOT / "tmp/xenon-more-lhbrx-output"
REPORT = ROOT / "logs/acv-dialect-audit-20261020/lhbrx-coverage.json"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    con = sqlite3.connect(DB)
    operation = con.execute(
        "SELECT id, exit_code, stdout, stderr FROM operations "
        "WHERE command_json LIKE '%acv-more-lhbrx.toml%' "
        "ORDER BY started_at DESC LIMIT 1").fetchone()
    if not operation or operation[1] != 0:
        raise RuntimeError("No successful isolated generator process in SQLite")
    op_id, exit_code, stdout, stderr = operation
    output = (stdout or "") + "\n" + (stderr or "")
    unrecognized = {m.group(1).lower() for m in re.finditer(
        r"Unrecognized instruction at (0x[0-9A-Fa-f]+):", output)}
    decoded = json.loads(DECODE.read_text())
    expected = {row["address"].lower() for row in decoded["classifications"]
                if row["xenia_classification"] == "lhbrx"}
    if len(expected) != 8 or len(unrecognized) != 26 or unrecognized & expected:
        raise RuntimeError("lhbrx coverage validation failed")
    expression_count = sum(
        path.read_text(errors="replace").count("__builtin_bswap16(PPC_LOAD_U16(")
        for path in OUTPUT_DIR.glob("*.cpp"))
    if expression_count != 8:
        raise RuntimeError(f"Expected eight emitted lhbrx expressions, got {expression_count}")
    report = {
        "scope": "isolated generation coverage; no complete binary build or runtime test",
        "base_commit": "9ff80d8321063657ea0e44edcc7ae6a60532e2c1",
        "local_changes": ["cell dialect clears PPC_OPCODE_403", "add PPC_INST_LHBRX handler using bswap16(PPC_LOAD_U16(ea))"],
        "recompiler_source": str(SOURCE.relative_to(ROOT)),
        "recompiler_source_sha256": sha256(SOURCE),
        "generator_binary_sha256": sha256(BUILD),
        "generator_sqlite_operation_id": op_id,
        "underlying_process_exit_code": exit_code,
        "wrapper_note": "diagnostic-bearing stdout causes wrapper status=failed despite exit code 0",
        "emitted_cpp_files": len(list(OUTPUT_DIR.glob("*.cpp"))),
        "remaining_unrecognized_addresses": len(unrecognized),
        "lhbrx_addresses": sorted(expected),
        "lhbrx_addresses_still_unrecognized": sorted(expected & unrecognized),
        "emitted_lhbrx_expressions": expression_count,
        "host_probe": "tools/acv_lhbrx_probe.cpp; three aligned halfwords; SQLite compile/run recorded",
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"unrecognized": len(unrecognized), "lhbrx_still_unrecognized": 0,
                      "emitted_lhbrx_expressions": expression_count}))
    print(f"Evidence: {REPORT}")


if __name__ == "__main__":
    main()
