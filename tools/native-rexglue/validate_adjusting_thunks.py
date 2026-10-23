#!/usr/bin/env python3
"""Independent check of the lwz r3,4(r3); b <target> thunk class.

Scans every aligned word pair in the original .text (not only the XenonRecomp
registry), records which pairs are referenced from non-text image words, and
compares the result with the generated C++ and the ReXGlue registration list.
"""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from xex_read_guest_range import extract_image

TEXT_BASE = 0x822B0000
raw = (ROOT / "games/acv/extracted/default.xex").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
assert sha == "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b"
image, base, _, _ = extract_image(raw)
text_size = (ROOT / "tmp/scan/text.bin").stat().st_size
text_lo, text_hi = TEXT_BASE, TEXT_BASE + text_size
gen = ROOT / "tmp/acv-rexglue-linux-indexed/generated/conditional-fragments"
registered = {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]+),",
                                             (gen / "acv_linux_probe_register.cpp").read_text())}
defined = set()
for path in gen.glob("acv_linux_probe_recomp.*.cpp"):
    defined |= {int(x, 16) for x in re.findall(r"^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)",
                                                 path.read_text(), re.M)}
cpp = (ROOT / "tmp/acv-native-adjusting-thunks.cpp").read_text()
leaves = (ROOT / "tools/native-rexglue/retail_leaves.cpp").read_text()
emitted = {int(a, 16): int(t, 16) for a, t in re.findall(
    r"static void AcvThunk([0-9A-F]{8})\(.*?\n.*?REX_LOAD_U32\(ctx\.r3\.u32 \+ 4\);\n\s+sub_([0-9A-F]{8})\(ctx, base\);",
    cpp)}
emitted.update({int(a, 16): int(t, 16) for a, t in re.findall(
    r"static void AcvLeaf([0-9A-F]{8})\(.*?\n.*?REX_LOAD_U32\(ctx\.r3\.u32 \+ 4\);\n\s+sub_([0-9A-F]{8})\(ctx, base\);",
    leaves)})
emitted_registered = set(re.findall(r"SetFunction\(0x([0-9A-F]{8}), Acv(?:Thunk|Leaf)", cpp + leaves))

pairs = {}
for off in range(text_lo - base, text_hi - base - 4, 4):
    load, branch = struct.unpack_from(">2I", image, off)
    if load == 0x80630004 and branch >> 26 == 18 and not branch & 3:
        delta = branch & 0x3FFFFFC
        if delta & 0x2000000:
            delta -= 0x4000000
        address = base + off
        prev = struct.unpack_from(">I", image, off - 4)[0]
        pairs[address] = {"target": address + 4 + delta, "prev_word": prev, "refs": []}
for index, (value,) in enumerate(struct.iter_unpack(">I", image[:len(image) // 4 * 4])):
    address = base + index * 4
    if value in pairs and not text_lo <= address < text_hi:
        pairs[value]["refs"].append(hex(address))

rows, problems = [], []
for address, item in sorted(pairs.items()):
    target = item["target"]
    row = {"address": hex(address), "target": hex(target), "prev_word": hex(item["prev_word"]),
           "nontext_references": item["refs"], "generated_registered": address in registered,
           "manual": address in emitted, "target_defined": target in defined,
           "target_registered": target in registered}
    rows.append(row)
    if address in emitted:
        if emitted[address] != target:
            problems.append(f"{address:#x}: emitted tail {emitted[address]:#x} != retail {target:#x}")
        if address in registered:
            problems.append(f"{address:#x}: duplicate registration with generated code")
        if target not in defined:
            problems.append(f"{address:#x}: target {target:#x} has no generated definition")
        if f"{address:08X}" not in emitted_registered:
            problems.append(f"{address:#x}: emitted but not registered")
    elif item["refs"] and address not in registered:
        problems.append(f"{address:#x}: referenced from data, neither generated nor emitted")
for address in emitted:
    if address not in pairs:
        problems.append(f"{address:#x}: emitted but retail bytes do not match class")

summary = {
    "scope": "original-byte class check; translation identity only, tail semantics unverified",
    "xex_sha256": sha, "text": [hex(text_lo), hex(text_hi)],
    "class_pairs_in_text": len(pairs),
    "already_generated_by_rexglue": sum(r["generated_registered"] for r in rows),
    "data_referenced_unregistered_before_fix": sum(bool(r["nontext_references"]) and not r["generated_registered"] for r in rows),
    "manually_emitted": len(emitted),
    "unreferenced_unregistered": [r["address"] for r in rows if not r["nontext_references"]
                                  and not r["generated_registered"] and not r["manual"]],
    "prev_word_histogram": {},
    "problems": problems, "rows": rows}
for r in rows:
    if r["manual"]:
        key = r["prev_word"]
        summary["prev_word_histogram"][key] = summary["prev_word_histogram"].get(key, 0) + 1
out = ROOT / "logs/acv-native-linux/adjusting-thunks-validation.json"
out.write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps({k: v for k, v in summary.items() if k != "rows"}, indent=2))
if problems:
    print("VALIDATION FAILED", file=sys.stderr)
    raise SystemExit(1)
