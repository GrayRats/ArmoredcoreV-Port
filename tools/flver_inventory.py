#!/usr/bin/env python3
"""Inventory FLVER headers of the extracted dvdbnd/BND3 trees with strict invariant checks.

The header layout (magic, endian marker, version, data offset/size, element counts,
bounding box) follows the public SoulsFormats FLVER2 reader; it is treated as a hypothesis
and every file is checked against structural invariants: data range inside the file,
finite bounding box with min <= max, plausible counts. Files that fail are reported with
the first failing invariant. Nothing here interprets vertex or face data.
"""

from __future__ import annotations

import csv
import math
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / "assets/original/acv-dvdbnd-all", ROOT / "assets/original/acv-bnd3-all",
           ROOT / "assets/original/acv-bnd3-nested"]
MAGIC = b"FLVER\0"


def inspect(blob: bytes) -> tuple[dict, str]:
    if len(blob) < 0x80 or blob[:6] != MAGIC:
        return {}, "magic/size"
    marker = blob[6:8]
    if marker not in (b"B\0", b"L\0"):
        return {}, "endian marker"
    e = ">" if marker == b"B\0" else "<"
    version, data_offset, data_size, dummies, materials, bones, meshes, vbuffers = struct.unpack_from(e + "8i", blob, 8)
    box = struct.unpack_from(e + "6f", blob, 0x28)
    faces, total_faces = struct.unpack_from(e + "2i", blob, 0x40)
    info = {"endian": marker[:1].decode(), "version": version, "data_offset": data_offset, "data_size": data_size,
            "dummies": dummies, "materials": materials, "bones": bones, "meshes": meshes,
            "vertex_buffers": vbuffers, "faces": faces, "total_faces": total_faces,
            "box": ",".join(f"{v:.4g}" for v in box)}
    if not 0 < version < 0x100000:
        return info, "version range"
    if data_offset < 0x80 or data_size < 0 or data_offset + data_size > len(blob):
        return info, "data range"
    if any(not 0 <= v <= 100_000 for v in (dummies, materials, bones, meshes, vbuffers)):
        return info, "count range"
    # An empty model stores the +/-FLT_MAX sentinel box (min > max) with zero meshes.
    if meshes == 0 and all(abs(v) >= 3.4e38 for v in box):
        info["note"] = "empty-model"
        return info, ""
    if any(not math.isfinite(v) for v in box) or any(box[i] > box[i + 3] for i in range(3)):
        return info, "bounding box"
    return info, ""


def main() -> int:
    out = ROOT / "assets/manifests/acv-flver-inventory.tsv"
    fields = ["file", "size", "endian", "version", "data_offset", "data_size", "dummies", "materials", "bones",
              "meshes", "vertex_buffers", "faces", "total_faces", "box", "note", "failure"]
    rows = []
    for source in SOURCES:
        for path in sorted(source.rglob("*")):
            if not path.is_file():
                continue
            with path.open("rb") as f:
                head = f.read(6)
            if head != MAGIC:
                continue
            blob = path.read_bytes()
            info, failure = inspect(blob)
            rows.append({"file": str(path.relative_to(ROOT)), "size": len(blob), "failure": failure, **info})
    with out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields, delimiter="\t", restval="")
        writer.writeheader()
        writer.writerows(rows)
    failures = Counter(r["failure"] or "ok" for r in rows)
    print(f"FLVER files {len(rows)}: {dict(failures)}")
    print("endian", dict(Counter(r.get("endian", "?") for r in rows)))
    print("versions", {hex(k): v for k, v in Counter(r.get("version") for r in rows if r.get("version") is not None).most_common(8)})
    print("empty models", sum(r.get("note") == "empty-model" for r in rows))
    ok = [r for r in rows if not r["failure"] and not r.get("note")]
    if ok:
        print(f"ok: meshes {min(r['meshes'] for r in ok)}..{max(r['meshes'] for r in ok)}, "
              f"bones {min(r['bones'] for r in ok)}..{max(r['bones'] for r in ok)}, "
              f"materials {min(r['materials'] for r in ok)}..{max(r['materials'] for r in ok)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
