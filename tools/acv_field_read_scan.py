#!/usr/bin/env python3
"""Find PPC D-form/DS-form loads with selected displacements in raw ACV text.

This is an offset-only candidate index. It does not infer the base object's
identity or pointer provenance and it does not modify the dumps/Ghidra database.
"""
import bisect
import json
import struct
import sys
from collections import Counter

TEXT_BASE = 0x822B0000
# Integer, floating-point, and load-reserve D-form/DS-form opcodes.
LOADS = {
    32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu",
    40: "lhz", 41: "lhzu", 42: "lha", 43: "lhau",
    46: "lmw", 48: "lfs", 49: "lfsu", 50: "lfd", 51: "lfdu",
}
DS_LOADS = {0: "ld", 1: "ldu", 2: "lwa"}


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: acv_field_read_scan.py TEXT PDATA OUT.json")
    text_path, pdata_path, out_path = sys.argv[1:]
    raw = open(text_path, "rb").read()
    words = struct.unpack(">%dI" % (len(raw) // 4), raw[:len(raw) // 4 * 4])
    pd = open(pdata_path, "rb").read()
    funcs = []
    for i in range(0, len(pd) - 7, 8):
        begin, word = struct.unpack_from(">II", pd, i)
        size = ((word >> 8) & 0x3FFFFF) * 4
        if size:
            funcs.append((begin, size))
    funcs.sort()
    starts = [b for b, _ in funcs]

    def owner(address):
        i = bisect.bisect_right(starts, address) - 1
        if i >= 0 and address < funcs[i][0] + funcs[i][1]:
            return funcs[i][0]
        return None

    offsets = {336, 352, 368, 384, 480, 484, 488, 492, 496, 500, 504, 508}
    offsets.update(range(512, 601, 8))
    matches = []
    for i, word in enumerate(words):
        op = word >> 26
        if op in LOADS:
            name = LOADS[op]
            disp = word & 0xFFFF
            if disp & 0x8000:
                disp -= 0x10000
        elif op == 58 and (word & 3) in DS_LOADS:
            name = DS_LOADS[word & 3]
            disp = word & 0xFFFC
            if disp & 0x8000:
                disp -= 0x10000
        else:
            continue
        if disp in offsets:
            at = TEXT_BASE + i * 4
            matches.append({
                "at": at,
                "op": name,
                "reg": (word >> 21) & 31,
                "base": (word >> 16) & 31,
                "disp": disp,
                "owner": owner(at),
            })
    result = {
        "text_base": TEXT_BASE,
        "offsets": sorted(offsets),
        "candidate_count": len(matches),
        "counts_by_offset": {str(k): v for k, v in sorted(Counter(m["disp"] for m in matches).items())},
        "candidates": matches,
        "limitation": "Matching displacement alone does not establish base-pointer identity or semantic use.",
    }
    with open(out_path, "w") as f:
        json.dump(result, f, indent=2)
        f.write("\n")
    print(f"candidate_loads={len(matches)} offsets={len(offsets)} out={out_path}")
    print(json.dumps(result["counts_by_offset"], sort_keys=True))


if __name__ == "__main__":
    main()
