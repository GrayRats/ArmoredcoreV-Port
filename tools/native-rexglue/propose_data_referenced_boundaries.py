#!/usr/bin/env python3
"""Propose and check boundaries for data-referenced entries missed by ReXGlue.

An entry is proposed only if a non-text image word points at it, the separate
XenonRecomp registry also lists it (proposal source only, no code is mixed),
and an original-byte CFG walk from the entry stays contiguous, reaches every
word of the interval and leaves only through blr/bctr or a direct tail branch
to an entry that is registered or itself accepted. Entries whose bytes are
already reached by the CFG of the preceding ReXGlue entry are rejected as
interior labels. Tail targets missing from ReXGlue are added transitively.
"""
import bisect
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
xex = ROOT / "games/acv/extracted/default.xex"
raw = xex.read_bytes()
assert hashlib.sha256(raw).hexdigest() == "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b"
image, base, _, _ = extract_image(raw)
text = (ROOT / "tmp/scan/text.bin").read_bytes()
text_lo, text_hi = TEXT_BASE, TEXT_BASE + len(text)
gen = ROOT / "tmp/acv-rexglue-linux-indexed/generated/conditional-fragments"
rex = {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]+),",
                                      (gen / "acv_linux_probe_register.cpp").read_text())}
manual_sources = [ROOT / "tools/native-rexglue/retail_leaves.cpp", ROOT / "tmp/acv-native-adjusting-thunks.cpp"]
manual = set()
for path in manual_sources:
    manual |= {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]{8}),", path.read_text())}
registry_path = ROOT / "tmp/xenon-more-mulhd-output/ppc_recomp_shared.h"
xenon = {int(x, 16) for x in re.findall(r"PPC_EXTERN_FUNC\(sub_([0-9A-Fa-f]{8})\)", registry_path.read_text())}
pdata = {}
for start, meta in struct.iter_unpack(">II", (ROOT / "tmp/scan/pdata.bin").read_bytes()):
    if (meta >> 8) & 0x3FFFFF:
        pdata[start] = start + ((meta >> 8) & 0x3FFFFF) * 4
# Entries added by this tool's earlier generation are re-derived, not "known", so
# the result is idempotent: baseline = the ReXGlue register saved before the first
# data-referenced generation (tmp/acv-rexglue-generated-backup-before-data-referenced).
_baseline = ROOT / "tmp/acv-rexglue-generated-backup-before-data-referenced/acv_linux_probe_register.cpp"
if _baseline.exists():
    rex &= {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]+),", _baseline.read_text())}
known = rex | manual


def word(pc):
    return struct.unpack_from(">I", text, pc - text_lo)[0]


def next_boundary(pc):
    i = bisect.bisect_right(boundaries, pc)
    return boundaries[i] if i < len(boundaries) else text_hi


branch_targets = set()  # targets of local branches seen by the latest walk


def walk(begin, limit):
    """Return (seen, exits, tails, error) for the CFG from begin below limit."""
    seen, pending, exits, tails = set(), [begin], [], set()
    branch_targets.clear()
    while pending:
        pc = pending.pop()
        if pc in seen:
            continue
        if not begin <= pc < limit:
            return seen, exits, tails, f"fallthrough/branch to {pc:#x} outside [{begin:#x},{limit:#x})"
        seen.add(pc)
        w = word(pc)
        op = w >> 26
        if w == 0:
            return seen, exits, tails, f"zero word reached at {pc:#x}"
        if op in (16, 18):
            mask, sign = (0xFFFC, 0x8000) if op == 16 else (0x3FFFFFC, 0x2000000)
            disp = w & mask
            if disp & sign:
                disp -= sign * 2
            target = (disp + (0 if w & 2 else pc)) & 0xFFFFFFFF
            uncond = op == 18 or ((w >> 21) & 20) == 20
            if w & 1:  # bl/bcl: call, continue after it
                pending.append(pc + 4)
                continue
            if begin <= target < limit:
                branch_targets.add(target)
                pending.append(target)
            elif op == 18 or target in known or target in refs:
                kind = "tail_branch" if op == 18 else "conditional_tail_branch"
                exits.append({"site": hex(pc), "target": hex(target), "kind": kind})
                tails.add(target)
            else:
                return seen, exits, tails, f"conditional branch at {pc:#x} leaves interval to {target:#x}"
            if not uncond:
                pending.append(pc + 4)
        elif op == 19 and ((w >> 1) & 1023) in (16, 528) and not w & 1:
            exits.append({"site": hex(pc), "kind": "lr_or_ctr_branch"})
            if ((w >> 21) & 20) != 20:
                pending.append(pc + 4)
        else:
            pending.append(pc + 4)
    return seen, exits, tails, None


refs = {}
for index, (value,) in enumerate(struct.iter_unpack(">I", image[:len(image) // 4 * 4])):
    address = base + index * 4
    if text_lo <= value < text_hi and not value & 3 and not text_lo <= address < text_hi and value not in known:
        refs.setdefault(value, []).append(hex(address))

data_refs = set(refs)
# Code-materialized pointers: lis rD,hi followed (before rD is redefined, within
# 512 words) by addi/ori rX,rD,lo. Used e.g. for MSVC pointer-to-virtual-member
# vcall thunks. Only a proposal source; the same registry/CFG checks apply.
STORE_OPS = {36, 37, 38, 39, 44, 45, 52, 53, 54, 55, 62}
X_STORE_XO = {151, 183, 215, 247, 407, 439, 663, 695, 727, 759, 918, 662, 150, 214, 149, 181, 231, 487}
TEXT_WORDS = struct.unpack(">%dI" % (len(text) // 4), text[:len(text) // 4 * 4])
for i, w in enumerate(TEXT_WORDS):
    if w >> 26 != 15 or (w >> 16) & 31:
        continue
    rd, hi = (w >> 21) & 31, (w & 0xFFFF) << 16
    for j in range(i + 1, min(i + 512, len(TEXT_WORDS))):
        v = TEXT_WORDS[j]
        op = v >> 26
        source = (v >> 21) & 31 if op == 24 else (v >> 16) & 31
        if op in (14, 24) and source == rd:
            lo = v & 0xFFFF
            value = (hi + (lo - 0x10000 if op == 14 and lo & 0x8000 else lo)) & 0xFFFFFFFF
            if text_lo <= value < text_hi and not value & 3 and value not in known:
                refs.setdefault(value, []).append(f"code:{text_lo + j * 4:#x}")
        if op in (16, 18) or (op == 19 and ((v >> 1) & 1023) in (16, 528)):
            break  # leave the basic block
        writes = (v >> 21) & 31 == rd and op not in STORE_OPS | {10, 11, 3, 2} and not (
            op == 31 and ((v >> 1) & 1023) in X_STORE_XO | {0, 32})
        if op in (24, 25, 26, 27, 28, 29, 21, 20, 23, 30) or (op == 31 and ((v >> 1) & 1023) in (28, 60, 124, 284, 316, 412, 444, 476, 24, 536, 792, 824, 26, 954, 922, 986)):
            writes = (v >> 16) & 31 == rd  # logical/shift/rotate forms write rA
        if writes:
            break

# XenonRecomp-only entries are treated as possible internal labels, not limits.
boundaries = sorted(known | set(pdata) | data_refs)
REX_SORTED = sorted(rex)
PDATA_STARTS = sorted(pdata)
accepted, rejected = {}, {}
queue = [(entry, "data_reference") for entry in sorted(refs)]
while queue:
    entry, reason = queue.pop(0)
    if entry in accepted or entry in rejected or entry in known:
        continue
    if entry not in xenon:
        rejected[entry] = {"reason": reason, "error": "not proposed by separate registry"}
        continue
    owner = PDATA_STARTS[bisect.bisect_right(PDATA_STARTS, entry) - 1] if PDATA_STARTS[0] <= entry else None
    if owner is not None and owner != entry and entry < pdata[owner]:
        rejected[entry] = {"reason": reason, "error": f"interior: inside .pdata [{owner:#x},{pdata[owner]:#x})"}
        continue
    # A code-only address formed solely inside its own enclosing function (between
    # the preceding ReXGlue entry and itself) is a local jump label, e.g. a switch base.
    sites = refs.get(entry, [])
    if sites and all(r.startswith("code:") for r in sites):
        prev_index = bisect.bisect_left(REX_SORTED, entry) - 1
        prev = REX_SORTED[prev_index] if prev_index >= 0 else text_lo
        if all(prev <= int(r[5:], 16) < entry for r in sites):
            rejected[entry] = {"reason": reason, "error": "interior: code-materialized only inside its own function"}
            continue
    # Interior test: CFG of the preceding ReXGlue entry, bounded by ReXGlue entries only.
    prev_index = bisect.bisect_left(REX_SORTED, entry) - 1
    if prev_index >= 0:
        prev = REX_SORTED[prev_index]
        nxt = REX_SORTED[prev_index + 1] if prev_index + 1 < len(REX_SORTED) else text_hi
        prev_seen, _, _, _ = walk(prev, nxt)
        # Reached only by falling through a call (e.g. a non-returning failure
        # handler) is a distinct entry; reached by a local branch is a label.
        after_call = word(entry - 4) >> 26 == 18 and word(entry - 4) & 3 == 1
        if entry in prev_seen and (entry in branch_targets or not after_call):
            rejected[entry] = {"reason": reason, "error": f"interior: reached by CFG of ReXGlue entry {prev:#x}"}
            continue
    seen, exits, tails, error = walk(entry, next_boundary(entry))
    if error is None:
        end = max(seen) + 4
        if seen != set(range(entry, end, 4)):
            error = f"unreached words in [{entry:#x},{end:#x})"
        elif not exits:
            error = "no exits"
    if error:
        rejected[entry] = {"reason": reason, "error": error}
        continue
    accepted[entry] = {"begin": hex(entry), "end": hex(end), "size": end - entry, "reason": reason,
                       "nontext_references": refs.get(entry, []), "exits": exits,
                       "sha256": hashlib.sha256(text[entry - text_lo:end - text_lo]).hexdigest()}
    for tail in sorted(tails):
        if tail not in known:
            queue.append((tail, f"tail_of_{entry:#x}"))

# A tail into a rejected entry has no target definition: reject until fixpoint.
changed = True
while changed:
    changed = False
    for entry, item in list(accepted.items()):
        bad = [e["target"] for e in item["exits"] if "target" in e
               and int(e["target"], 16) not in known and int(e["target"], 16) not in accepted]
        if bad:
            rejected[entry] = {"reason": item["reason"], "error": f"tail target(s) without definition: {bad}"}
            del accepted[entry]
            changed = True
# Accepted intervals must not overlap each other or registered entries.
ordered = sorted(accepted.values(), key=lambda r: int(r["begin"], 16))
for left, right in zip(ordered, ordered[1:]):
    assert int(left["end"], 16) <= int(right["begin"], 16), f"overlap {left['begin']} {right['begin']}"
for r in ordered:
    b, e = int(r["begin"], 16), int(r["end"], 16)
    i = bisect.bisect_right(REX_SORTED, b)
    assert i == len(REX_SORTED) or REX_SORTED[i] >= e, f"{r['begin']} overlaps ReXGlue entry"

config = ROOT / "tools/native-rexglue/acv-data-referenced-functions.toml"
lines = ["# Data-referenced entries missing from ReXGlue discovery; generated by",
         "# tools/native-rexglue/propose_data_referenced_boundaries.py. Original-byte CFG",
         "# checked; not recovered symbols, retail ownership or behavior parity.",
         "# Evidence: logs/acv-native-linux/data-referenced-boundaries.json", "[functions]"]
lines += [f"{r['begin']} = {{ size = {r['size']} }}" for r in ordered]
config.write_text("\n".join(lines) + "\n")
report = {
    "scope": "instruction-derived analysis boundaries; codegen/runtime behavior unverified",
    "xex_sha256": hashlib.sha256(raw).hexdigest(),
    "registry_sha256": hashlib.sha256(registry_path.read_bytes()).hexdigest(),
    "proposer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "config_sha256": hashlib.sha256(config.read_bytes()).hexdigest(),
    "data_referenced_unregistered": len(refs), "accepted": len(ordered),
    "accepted_words": sum(r["size"] for r in ordered) // 4, "rejected": len(rejected),
    "rejected_by_error": {},
    "functions": ordered,
    "rejected_entries": {hex(k): v for k, v in sorted(rejected.items())}}
for v in rejected.values():
    key = v["error"].split(":")[0].split(" at ")[0].split(" to ")[0]
    report["rejected_by_error"][key] = report["rejected_by_error"].get(key, 0) + 1
(ROOT / "logs/acv-native-linux/data-referenced-boundaries.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({k: report[k] for k in ("data_referenced_unregistered", "accepted", "accepted_words",
                                          "rejected", "rejected_by_error")}, indent=2))
