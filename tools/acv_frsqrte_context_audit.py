#!/usr/bin/env python3
"""Disassemble small windows around the remaining frsqrte report sites.

Reads the existing text dump and report JSON only. It does not connect to a
running emulator or modify Ghidra/XEX data.
"""
import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DUMP_BASE = 0x822B0000


def disassemble(words):
    source = "\n".join(" ".join(f"0x{b:02x}" for b in w) for w in words) + "\n"
    p = subprocess.run(
        ["llvm-mc", "--disassemble", "--triple=powerpc64-unknown-unknown", "-mcpu=pwr7"],
        input=source, capture_output=True, text=True, check=False,
    )
    invalid = {int(m.group(1)) for m in re.finditer(
        r"<stdin>:(\d+):\d+: warning: invalid instruction", p.stderr
    )}
    decoded = [line.strip() for line in p.stdout.splitlines()
               if line.strip() and not line.strip().startswith(".text")]
    out = []
    it = iter(decoded)
    for n, word in enumerate(words, 1):
        out.append(f".long 0x{word.hex()}" if n in invalid else next(it, "<missing>"))
    return out, p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", default="tmp/scan/text.bin")
    ap.add_argument("--reports", default="logs/acv-more-instructions-20261020/remaining-instructions.json")
    ap.add_argument("--output", default="logs/acv-dialect-audit-20261020/frsqrte-contexts.json")
    ap.add_argument("--radius", type=int, default=12, help="instruction words on each side")
    a = ap.parse_args()
    blob = (ROOT / a.dump).read_bytes()
    reports = json.loads((ROOT / a.reports).read_text())
    targets = [r for r in reports if r["reported_mnemonic"] == "frsqrte"]
    result = {
        "scope": "static context disassembly from existing text dump; LLVM pwr7 display only; not Xenon VMX/FPU semantic proof",
        "dump": a.dump,
        "dump_base": hex(DUMP_BASE),
        "radius_words": a.radius,
        "targets": [],
    }
    for r in targets:
        addr = int(r["address"], 16)
        lo = max(DUMP_BASE, addr - a.radius * 4)
        hi = min(DUMP_BASE + len(blob), addr + (a.radius + 1) * 4)
        lo = (lo + 3) & ~3
        start_i = lo - DUMP_BASE
        words = [blob[i:i+4] for i in range(start_i, hi-DUMP_BASE, 4)]
        asm, stderr = disassemble(words)
        lines = []
        target_i = (addr - lo) // 4
        if words[target_i].hex() != r["raw_word"]:
            raise SystemExit(
                f"raw word mismatch at {addr:#x}: dump={words[target_i].hex()} report={r['raw_word']}"
            )
        for i, (raw, text) in enumerate(zip(words, asm)):
            lines.append({"address": hex(lo + 4*i), "raw_word": raw.hex(), "assembly": text,
                          "target": i == target_i})
        target_asm = asm[target_i]
        operands = re.search(r"^frsqrte\s+(\d+),\s*(\d+)$", target_asm)
        following = lines[target_i + 1:target_i + 5]
        result["targets"].append({
            "address": hex(addr), "reported_mnemonic": r["reported_mnemonic"],
            "raw_word": r["raw_word"], "pdata_owners": r["pdata_owners"],
            "destination_fpr": int(operands.group(1)) if operands else None,
            "source_fpr": int(operands.group(2)) if operands else None,
            "next_four_instructions": following,
            "window": lines, "llvm_diagnostics": stderr.strip(),
        })
    out = ROOT / a.output
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n")
    print(f"targets={len(targets)} output={out.relative_to(ROOT)}")
    for t in result["targets"]:
        target = next(x for x in t["window"] if x["target"])
        print(f"{t['address']} owner={t['pdata_owners'][0]['start']} {target['assembly']}")


if __name__ == "__main__":
    main()
