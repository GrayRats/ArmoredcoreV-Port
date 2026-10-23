#!/usr/bin/env python3
"""Check isolated mulhdx generator coverage and retained output."""
import hashlib
import json
from pathlib import Path
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "logs/decomp.sqlite3"
DECODE = ROOT / "logs/acv-dialect-audit-20261020/xenia-decode-audit.json"
SOURCE = ROOT / "tmp/xenon-more-mulhd-source/XenonRecomp/recompiler.cpp"
BUILD = ROOT / "tmp/xenon-more-mulhd-build/XenonRecomp/XenonRecomp"
OUTPUT_DIR = ROOT / "tmp/xenon-more-mulhd-output"
REPORT = ROOT / "logs/acv-dialect-audit-20261020/mulhdx-coverage.json"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    con = sqlite3.connect(DB)
    op = con.execute(
        "SELECT id, exit_code, stdout, stderr FROM operations "
        "WHERE command_json LIKE '%acv-more-mulhd.toml%' "
        "ORDER BY started_at DESC LIMIT 1").fetchone()
    if not op or op[1] != 0:
        raise RuntimeError("No successful isolated generator process in SQLite")
    op_id, exit_code, stdout, stderr = op
    text = (stdout or "") + "\n" + (stderr or "")
    unrecognized = {m.group(1).lower() for m in re.finditer(
        r"Unrecognized instruction at (0x[0-9A-Fa-f]+):", text)}
    decoded = json.loads(DECODE.read_text())
    addresses = {r["address"].lower() for r in decoded["classifications"]
                 if r["xenia_classification"] == "mulhdx"}
    emitted = [line.strip() for path in OUTPUT_DIR.glob("*.cpp")
               for line in path.read_text(errors="replace").splitlines()
               if "__int128_t(" in line and ">> 64;" in line]
    if len(addresses) != 2 or len(unrecognized) != 24 or unrecognized & addresses or len(emitted) != 2:
        raise RuntimeError("mulhdx coverage validation failed")
    report = {
        "scope": "isolated codegen coverage; no full shard build or runtime execution",
        "base_commit": "9ff80d8321063657ea0e44edcc7ae6a60532e2c1",
        "local_changes": ["cell dialect clears PPC_OPCODE_403", "vnor128 handler", "lhbrx handler", "mulhdx handler using signed __int128 high half"],
        "recompiler_source": str(SOURCE.relative_to(ROOT)),
        "recompiler_source_sha256": sha256(SOURCE),
        "generator_binary_sha256": sha256(BUILD),
        "generator_sqlite_operation_id": op_id,
        "underlying_process_exit_code": exit_code,
        "wrapper_note": "diagnostic-bearing stdout triggers wrapper failure heuristic",
        "mulhdx_addresses": sorted(addresses),
        "mulhdx_addresses_still_unrecognized": sorted(addresses & unrecognized),
        "remaining_unrecognized_addresses": len(unrecognized),
        "emitted_mulhdx_expressions": emitted,
        "emitted_cpp_files": len(list(OUTPUT_DIR.glob("*.cpp"))),
        "host_probe": "tools/acv_mulhdx_probe.cpp; 9 signed edge/mixed vectors; SQLite compile/run recorded",
    }
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"unrecognized": len(unrecognized), "mulhdx_still_unrecognized": 0,
                      "emitted_mulhdx_expressions": len(emitted)}))
    print(f"Evidence: {REPORT}")


if __name__ == "__main__":
    main()
