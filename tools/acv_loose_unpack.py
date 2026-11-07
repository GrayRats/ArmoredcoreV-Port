#!/usr/bin/env python3
"""Unpack the ACV Xbox 360 dvdbnd5.bhd/dvdbnd.bdt archive into loose files.

Replacement for the main-archive step of ACVLooseLoader, whose BHD5 reader
produces no entries for the observed big-endian 16-byte-entry variant. Reuses
parse() from bhd5_index.py. Names come from a dictionary (one path per line,
hashed as lowercase with a leading '/'); unknown entries go to _unknown/<hash>.
Raw entry bytes are written unchanged (DCX stays compressed). The input
archive is opened read-only; the output root must be a staging copy.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import sys
from pathlib import Path


def load_parse():
    spec = importlib.util.spec_from_file_location("bhd5_index", Path(__file__).with_name("bhd5_index.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.parse


def path_hash(path: str) -> int:
    value = 0
    for char in path:
        value = (value * 37 + ord(char)) & 0xFFFFFFFF
    return value


def load_dictionary(path: Path) -> dict[int, list[str]]:
    names: dict[int, list[str]] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.strip().replace("\\", "/").lower()
        if not line:
            continue
        if not line.startswith("/"):
            line = "/" + line
        bucket = names.setdefault(path_hash(line), [])
        if line not in bucket:
            bucket.append(line)
    return names


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--dict", required=True, type=Path, dest="dictionary")
    ap.add_argument("--root", required=True, type=Path, help="staging root to write files into")
    ap.add_argument("--manifest", type=Path, help="write TSV: path, size, sha256, status")
    ap.add_argument("--overwrite", action="store_true", help="replace existing files (default: skip)")
    args = ap.parse_args()

    root = args.root.resolve()
    if not root.is_dir():
        print(f"root is not a directory: {root}", file=sys.stderr)
        return 2
    sources = {args.bhd.resolve(), args.bdt.resolve()}

    entries = load_parse()(args.bhd, args.bdt)
    names = load_dictionary(args.dictionary)
    rows = []
    stats = {"named": 0, "unknown": 0, "skipped": 0, "collision": 0}
    with args.bdt.open("rb") as bdt:
        for entry in entries:
            candidates = names.get(entry["path_hash"], [])
            if candidates:
                relative = candidates[0].lstrip("/")
                if len(candidates) > 1:
                    stats["collision"] += 1
                stats["named"] += 1
            else:
                relative = f"_unknown/{entry['path_hash']:08x}"
                stats["unknown"] += 1
            target = (root / relative).resolve()
            if root not in target.parents:
                print(f"skipping path outside root: {relative}", file=sys.stderr)
                continue
            if target in sources:
                raise ValueError(f"entry would overwrite source archive: {relative}")
            bdt.seek(entry["offset"])
            data = bdt.read(entry["size"])
            if len(data) != entry["size"]:
                raise ValueError(f"short read for hash {entry['path_hash']:08x}")
            if target.exists() and not args.overwrite:
                stats["skipped"] += 1
                status = "exists"
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                status = "written"
            rows.append((relative, entry["size"], hashlib.sha256(data).hexdigest(), status))

    if args.manifest:
        with args.manifest.open("w", newline="") as handle:
            writer = csv.writer(handle, delimiter="\t")
            writer.writerow(["path", "size", "sha256", "status"])
            writer.writerows(rows)
    print(f"entries={len(entries)} {stats}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
