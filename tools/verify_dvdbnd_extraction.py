#!/usr/bin/env python3
"""Independently verify bhd5_extract_all.py output against dvdbnd and the older BND3 extraction.

Checks: unique output names (path-hash collisions would overwrite files), every output
file hashed against the manifest, raw (non-DCX) entries compared byte-for-byte with their
BDT slice, DCX entries compared with the declared raw size, and the BND3 files.tsv hashes
against the earlier acv-bnd3-extracted manifest for the same containers.
"""

from __future__ import annotations

import csv
import hashlib
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import parse  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ALL = ROOT / "assets/original/acv-dvdbnd-all"


def main() -> int:
    manifest = list(csv.DictReader((ROOT / "assets/manifests/acv-dvdbnd-all.tsv").open(encoding="utf-8"), delimiter="\t"))
    entries = parse(ROOT / "tmp/iso_root/bind/dvdbnd5.bhd", ROOT / "tmp/iso_root/bind/dvdbnd.bdt")
    problems = 0

    hashes = Counter(int(e["path_hash"]) for e in entries)
    duplicates = {h: n for h, n in hashes.items() if n > 1}
    names = Counter(r["file"] for r in manifest if r["file"])
    shared = {n: c for n, c in names.items() if c > 1}
    print(f"entries {len(entries)}, unique path hashes {len(hashes)}, duplicate hashes {len(duplicates)}")
    print(f"manifest rows {len(manifest)}, unique output names {len(names)}, shared names {len(shared)}")
    on_disk = sum(1 for p in ALL.iterdir() if p.is_file())
    print(f"files on disk {on_disk}")
    problems += bool(duplicates) + bool(shared) + (on_disk != len(names))

    bad_hash = missing = raw_mismatch = dcx_size = 0
    with (ROOT / "tmp/iso_root/bind/dvdbnd.bdt").open("rb") as bdt:
        for e, row in zip(entries, manifest):
            path = ALL / row["file"]
            if not path.is_file():
                missing += 1
                continue
            data = path.read_bytes()
            if hashlib.sha256(data).hexdigest() != row["sha256"]:
                bad_hash += 1
            bdt.seek(int(e["offset"]))
            stored = bdt.read(int(e["size"]))
            if stored[:4] == b"DCX\0":
                if struct.unpack_from(">I", stored, 0x1C)[0] != len(data):
                    dcx_size += 1
            elif stored != data:
                raw_mismatch += 1
    print(f"missing {missing}, manifest hash mismatch {bad_hash}, raw != BDT slice {raw_mismatch}, DCX size mismatch {dcx_size}")
    problems += bool(missing or bad_hash or raw_mismatch or dcx_size)

    old = ROOT / "assets/original/acv-bnd3-extracted/files.tsv"
    new = ROOT / "assets/original/acv-bnd3-all/files.tsv"
    if old.is_file() and new.is_file():
        old_rows = {(r["container_hash"], r["index"]): r["sha256"]
                    for r in csv.DictReader(old.open(encoding="utf-8"), delimiter="\t")}
        new_rows = {(r["container_hash"], r["index"]): r["sha256"]
                    for r in csv.DictReader(new.open(encoding="utf-8"), delimiter="\t")}
        common = old_rows.keys() & new_rows.keys()
        differ = sum(old_rows[k] != new_rows[k] for k in common)
        print(f"BND3 older extraction {len(old_rows)} files, new {len(new_rows)}, common {len(common)}, differing hashes {differ}, "
              f"older-only {len(old_rows.keys() - new_rows.keys())}")
        problems += bool(differ) + bool(old_rows.keys() - new_rows.keys())
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
