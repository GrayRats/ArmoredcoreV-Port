#!/usr/bin/env python3
"""Extract every BHD5 entry of dvdbnd into the workspace, decoding validated DCX/DCP DFLT.

Reuses parse() and decode_dcx_dflt() from bhd5_index.py. Output names carry the 32-bit
path hash and a extension chosen from the decoded magic; original filenames are not
recovered. Entries that fail a strict check are recorded in the manifest with status
"error" and are not written. Output stays inside the workspace.
"""

from __future__ import annotations

import csv
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import decode_dcx_dflt, parse  # noqa: E402

WORKSPACE = Path(__file__).resolve().parents[1]
SIGNATURE_EXTENSIONS = {
    b"TPF\0": "tpf",
    b"BND3": "bnd3",
    b"FLVE": "flver",
    b"TAE ": "tae",
    b"SMD4": "smd4",
    b"FEV1": "fev",
    b"<?xm": "xml",
    b"DCX\0": "dcx",
    b"\x00BRD": "brd",
    b"DDS ": "dds",
    b"RIFF": "wav",
    b"OggS": "ogg",
}


def extension_for(blob: bytes) -> str:
    for magic, ext in SIGNATURE_EXTENSIONS.items():
        if blob[: len(magic)] == magic:
            return ext
    return "bin"


def main() -> int:
    bhd = WORKSPACE / "tmp/iso_root/bind/dvdbnd5.bhd"
    bdt = WORKSPACE / "tmp/iso_root/bind/dvdbnd.bdt"
    out_dir = WORKSPACE / "assets/original/acv-dvdbnd-all"
    manifest = WORKSPACE / "assets/manifests/acv-dvdbnd-all.tsv"
    if not out_dir.resolve().is_relative_to(WORKSPACE) or not manifest.resolve().is_relative_to(WORKSPACE):
        raise SystemExit("output must stay inside workspace")
    out_dir.mkdir(parents=True, exist_ok=True)

    entries = parse(bhd, bdt)
    fields = ["path_hash", "bucket", "entry", "stored_size", "decoded", "decoded_size", "kind", "status", "sha256", "file"]
    counts: dict[str, int] = {}
    with manifest.open("w", newline="", encoding="utf-8") as mf, bdt.open("rb") as src:
        writer = csv.DictWriter(mf, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        for e in entries:
            src.seek(int(e["offset"]))
            blob = src.read(int(e["size"]))
            decoded = False
            status = "ok"
            if blob[:4] == b"DCX\0":
                try:
                    blob = decode_dcx_dflt(blob)
                    decoded = True
                except ValueError as exc:
                    status = f"error:{exc}"
            if status != "ok":
                writer.writerow({"path_hash": f"{int(e['path_hash']) & 0xFFFFFFFF:08x}", "bucket": e["bucket"],
                                 "entry": e["entry"], "stored_size": e["size"], "decoded": decoded,
                                 "decoded_size": "", "kind": "", "status": status, "sha256": "", "file": ""})
                counts["error"] = counts.get("error", 0) + 1
                continue
            ext = extension_for(blob)
            name = f"{int(e['path_hash']) & 0xFFFFFFFF:08x}.{ext}"
            (out_dir / name).write_bytes(blob)
            counts[ext] = counts.get(ext, 0) + 1
            writer.writerow({"path_hash": f"{int(e['path_hash']) & 0xFFFFFFFF:08x}", "bucket": e["bucket"],
                             "entry": e["entry"], "stored_size": e["size"], "decoded": decoded,
                             "decoded_size": len(blob), "kind": blob[:4].hex(), "status": status,
                             "sha256": hashlib.sha256(blob).hexdigest(), "file": name})
    for ext, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"{n:6d} {ext}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
