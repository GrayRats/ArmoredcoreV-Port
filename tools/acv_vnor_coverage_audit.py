#!/usr/bin/env python3
"""Check isolated generator coverage after adding only the vnor128 handler."""
import hashlib
import json
from pathlib import Path
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "logs/decomp.sqlite3"
DECODE = ROOT / "logs/acv-dialect-audit-20261020/xenia-decode-audit.json"
SOURCE = ROOT / "tmp/xenon-more-vnor-source/XenonRecomp/recompiler.cpp"
BUILD = ROOT / "tmp/xenon-more-vnor-build/XenonRecomp/XenonRecomp"
OUTPUT_DIR = ROOT / "tmp/xenon-more-vnor-output"
REPORT = ROOT / "logs/acv-dialect-audit-20261020/vnor128-coverage.json"
EXPECTED = {"0x82c62538", "0x82c625c8", "0x82f8e208"}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    con = sqlite3.connect(DB)
    operation = con.execute(
        "SELECT id, exit_code, stdout, stderr FROM operations "
        "WHERE command_json LIKE '%xenon-more-vnor-build%' "
        "ORDER BY started_at DESC LIMIT 1").fetchone()
    if not operation or operation[1] != 0:
        raise RuntimeError("No successful isolated generator process in SQLite")
    op_id, exit_code, stdout, stderr = operation
    text = (stdout or "") + "\n" + (stderr or "")
    reported = {m.group(1).lower() for m in re.finditer(
        r"Unrecognized instruction at (0x[0-9A-Fa-f]+):", text)}
    decoded = json.loads(DECODE.read_text())
    actual_vnor = {row["address"].lower() for row in decoded["classifications"]
                   if row["xenia_classification"] == "vnor128"}
    if actual_vnor != EXPECTED or reported & EXPECTED:
        raise RuntimeError("vnor128 coverage check failed")
    if len(reported) != 35:
        raise RuntimeError(f"Expected 35 remaining unrecognized addresses, got {len(reported)}")
    emitted_vnor_expressions = sum(
        path.read_text(errors="replace").count("_mm_xor_si128(_mm_or_si128(")
        for path in OUTPUT_DIR.glob("*.cpp"))
    if emitted_vnor_expressions != 3:
        raise RuntimeError(f"Expected 3 emitted vnor expressions, got {emitted_vnor_expressions}")

    report = {
        "scope": "isolated generator coverage only; not instruction semantic or runtime verification",
        "xenonrecomp_base_commit": "9ff80d8321063657ea0e44edcc7ae6a60532e2c1",
        "local_change": "add PPC_INST_VNOR128 dispatch handler in isolated copy; no other handler changes",
        "source_path": str(SOURCE.relative_to(ROOT)),
        "source_sha256": sha256(SOURCE),
        "generator_binary_sha256": sha256(BUILD),
        "generator_sqlite_operation_id": op_id,
        "underlying_process_exit_code": exit_code,
        "wrapper_status_note": "SQLite wrapper marks diagnostic-bearing stdout as failed although process exit code is 0",
        "emitted_cpp_files": len(list(OUTPUT_DIR.glob("*.cpp"))),
        "remaining_unrecognized_addresses": len(reported),
        "vnor128_addresses": sorted(actual_vnor),
        "vnor128_addresses_still_unrecognized": sorted(reported & EXPECTED),
        "emitted_vnor_expressions": emitted_vnor_expressions,
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"unrecognized": len(reported), "vnor128_still_unrecognized": 0,
                      "emitted_vnor_expressions": emitted_vnor_expressions,
                      "emitted_cpp_files": report["emitted_cpp_files"]}))
    print(f"Evidence: {REPORT}")


if __name__ == "__main__":
    main()
