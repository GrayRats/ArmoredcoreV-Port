#!/usr/bin/env python3
"""Confirm which TPF texture a bound replay texture is, by exact block equality.

Replay textures are RenderDoc linear BC1 blocks; TPF payloads are Xenos tiled blocks (16-bit
swapped). Untiling uses the crunch-style 2D layout from tools/rexglue-sdk conversion.cpp with
block coordinates and log2 = 3 (BC1), which matched all 16 384 blocks of the bound texture in
frame 73703 (see tools/bc_tile_match.py). Output is a JSON evidence file written inside the workspace.
"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bc_tile_match import hypotheses, swap16  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
TEX_DIR = ROOT / "assets/original/acv-dvdbnd-textures"
CAPTURE_DIR = ROOT / "logs/acv-renderdoc/20261021T215316118397Z"


def untile_bc1(payload: bytes, width: int) -> bytes:
    bw = width // 4
    nb = bw * bw
    transform = hypotheses(bw, width)["crunch_blocks"]
    out = bytearray(nb * 8)
    for k in range(nb):
        o = transform(k % bw, k // bw)
        out[k * 8:k * 8 + 8] = swap16(payload[o:o + 8])
    return bytes(out)


def main() -> int:
    rows = list(csv.DictReader((TEX_DIR / "textures.tsv").open(encoding="utf-8"), delimiter="\t"))
    checks = [
        ("texture19_event7180", "tex19_73703.bin", 512, "677aa4df_0002_m0200_006.x360tex"),
        ("texture20_event7180", "tex20_73703.bin", 256, "677aa4df_0003_m0200_006_s.x360tex"),
    ]
    report = {"capture": "acv_frame73703.rdc", "scope": "bound BC1 blocks equal to TPF payload after untiling",
              "checks": []}
    for label, linear_name, width, tpf_name in checks:
        linear = (ROOT / "tmp/bc_untile" / linear_name).read_bytes()
        payload = (TEX_DIR / tpf_name).read_bytes()
        expected = sum(1 for _ in range(len(linear) // 8))
        untiled = untile_bc1(payload, width)
        same = sum(untiled[i:i + 8] == linear[i:i + 8] for i in range(0, len(linear), 8))
        meta = next(r for r in rows if r["file"] == tpf_name)
        report["checks"].append({"label": label, "tpf_file": tpf_name, "texture_name": meta["texture_name"],
                                 "width": width, "matching_blocks": same, "total_blocks": expected,
                                 "exact": untiled == linear})
    out = CAPTURE_DIR / "texture_identity.json"
    out.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    for c in report["checks"]:
        print(f"{c['label']}\t{c['texture_name']}\t{c['matching_blocks']}/{c['total_blocks']}\texact={c['exact']}")
    return 0 if all(c["exact"] for c in report["checks"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
