#!/usr/bin/env python3
"""Validate ACV analysis hints against original instructions, not retail behavior."""
import argparse, hashlib, json, re, struct, subprocess, sys, tomllib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from xex_read_guest_range import extract_image
ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--manifest", type=Path)
args = parser.parse_args()
source = ROOT / "tools/native-rexglue/acv-boundary-candidates.json"
xex = ROOT / "games/acv/extracted/default.xex"
xex_bytes = xex.read_bytes()
assert hashlib.sha256(xex_bytes).hexdigest() == "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b"
image, base, _, _ = extract_image(xex_bytes)
assert base == 0x82000000
records = json.loads(source.read_text())
registry_path = ROOT / "tmp/xenon-more-mulhd-output/ppc_recomp_shared.h"
registry = set(int(a,16) for a in re.findall(r"PPC_EXTERN_FUNC\(sub_([0-9A-Fa-f]{8})\)", registry_path.read_text()))
results = []
previous_end = 0
for r in records:
    begin, end = int(r["begin"], 16), int(r["end"], 16)
    assert begin < end and (end-begin) % 4 == 0
    assert begin >= previous_end, "Overlapping hint intervals"
    previous_end = end
    disassembly = subprocess.check_output([sys.executable, str(ROOT / "tools/ppc_dis.py"), str(ROOT / "tmp/scan/text.bin"), hex(begin), hex(end)], text=True)
    assert ".long" not in disassembly, "Unrecognized instruction in boundary hint"
    code = image[begin-base:end-base]
    assert len(code) == end-begin
    seen, pending, exits = set(), [begin], []
    while pending:
        pc = pending.pop()
        if pc in seen:
            continue
        assert begin <= pc < end, f"Fallthrough outside hint: {pc:#x}"
        seen.add(pc)
        w = struct.unpack_from(">I", code, pc-begin)[0]
        op = w >> 26
        assert w != 0, f"Zero opcode reached: {pc:#x}"
        successor = [pc+4]
        if op in (16, 18):
            mask, sign = (0xfffc, 0x8000) if op == 16 else (0x3fffffc, 0x2000000)
            disp = w & mask
            if disp & sign:
                disp -= sign*2
            target = (disp + (0 if w & 2 else pc)) & 0xffffffff
            if not w & 1:
                if begin <= target < end:
                    successor = [target]
                else:
                    assert op == 18, "Conditional branch leaves hint"
                    assert target in registry, "Tail target lacks a proposed entry in separate registry"
                    exits.append({"site": hex(pc), "target": hex(target), "kind": "tail_branch"})
                    successor = []
                if op == 16:
                    successor.append(pc+4)
        elif op == 19 and ((w >> 1) & 1023) in (16, 528) and not w & 1:
            bo = (w >> 21) & 31
            exits.append({"site": hex(pc), "kind": "lr_or_ctr_branch"})
            successor = [] if (bo & 20) == 20 else [pc+4]
        pending.extend(successor)
    assert seen == set(range(begin, end, 4)), f"Unreached words in {begin:#x} hint"
    assert exits, f"No exits in {begin:#x}"
    results.append({**r, "instruction_count": len(seen), "sha256": hashlib.sha256(code).hexdigest(), "exits": exits})
config = ROOT / "tools/native-rexglue/acv-boundaries.toml"
lines = ["# Analysis hints: original-byte CFG checked; not recovered symbols or retail parity.",
         "# Evidence: logs/acv-rexglue-unresolved-20261021/boundary-validation.json", "[functions]"]
for r in results:
    lines.append(f"{r['begin']} = {{ size = {r['size']} }}")
config_text = "\n".join(lines)+"\n"
if not config.exists() or config.read_text() != config_text:
    config.write_text(config_text)
parsed = tomllib.loads(config.read_text())
assert len(parsed["functions"]) == len(results) == len(records)
report = {"confidence": "instruction-derived analysis boundaries; codegen/runtime behavior unverified", "xex_sha256": hashlib.sha256(xex.read_bytes()).hexdigest(), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "registry_sha256": hashlib.sha256(registry_path.read_bytes()).hexdigest(), "validator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "config_sha256": hashlib.sha256(config.read_bytes()).hexdigest(), "instructions": sum(r["instruction_count"] for r in results), "functions": results}
(ROOT / "logs/acv-rexglue-unresolved-20261021/boundary-validation.json").write_text(json.dumps(report, indent=2)+"\n")
if args.manifest:
    manifest = args.manifest.resolve()
    assert manifest.is_relative_to(ROOT / "tmp")
    text = manifest.read_text()
    assert "includes = []" in text
    manifest.write_text(text.replace("includes = []", "includes = ["+json.dumps(str(config))+"]", 1))
print(json.dumps({"hints": len(results), "reachable_instructions": report["instructions"], "manifest_updated": bool(args.manifest)}))
