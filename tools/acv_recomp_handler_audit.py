#!/usr/bin/env python3
"""Record dispatch-handler presence for the remaining ACV PPC instruction families."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERIFY = ROOT / "logs/acv-more-instructions-20261020/verification.json"
SOURCE = ROOT / "tmp/xenon-more-source/XenonRecomp/recompiler.cpp"
OUTPUT = ROOT / "logs/acv-dialect-audit-20261020/recompiler-handler-audit.json"

FAMILIES = {
    "frsqrtex": "PPC_INST_FRSQRTE",
    "vnor128": "PPC_INST_VNOR128",
    "vsldoi128": "PPC_INST_VSLDOI128",
    "lhbrx": "PPC_INST_LHBRX",
    "mulhdx": "PPC_INST_MULHD",
}


def main():
    verification = json.loads(VERIFY.read_text())
    source_bytes = SOURCE.read_bytes()
    source = source_bytes.decode("utf-8")
    rows = []
    for family, opcode_id in FAMILIES.items():
        needle = f"case {opcode_id}:"
        lines = [i for i, line in enumerate(source.splitlines(), 1) if needle in line]
        rows.append({"xenia_family": family, "recompiler_opcode_id": opcode_id,
                     "dispatch_case_lines": lines, "handler_present": bool(lines)})

    report = {
        "scope": "static case-label presence only; does not prove emitted semantics or runtime behavior",
        "xenonrecomp_commit": verification["commit"],
        "branch_source_note": "isolated BBB_MoreInstructions tree; recompiler.cpp was not modified by build-only linkage fix",
        "source_path": str(SOURCE.relative_to(ROOT)),
        "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "family_dispatch_cases": rows,
        "summary": {"families": len(rows), "with_dispatch_case": sum(r["handler_present"] for r in rows),
                    "without_dispatch_case": sum(not r["handler_present"] for r in rows)},
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["summary"]))
    print(f"Evidence: {OUTPUT}")


if __name__ == "__main__":
    main()
