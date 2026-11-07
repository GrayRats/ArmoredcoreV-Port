#!/usr/bin/env python3
"""Move `_unknown/<hash>` loose files to real names discovered from runtime logs.

Reads `[NtCreateFile] FAILED: path='game:\\...'` lines from one or more logs,
hashes each requested path (lowercase, '/' separators, leading '/'), and when a
file `_unknown/<hash>` exists in the staging root, moves it to the requested
(lowercase) path. Optionally appends the discovered names to a dictionary file.
Works only inside the given staging root.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

PATTERN = re.compile(r"FAILED: path='game:\\([^']+)'")


def path_hash(path: str) -> int:
    value = 0
    for char in path:
        value = (value * 37 + ord(char)) & 0xFFFFFFFF
    return value


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", required=True, type=Path)
    ap.add_argument("--dict", type=Path, help="append discovered names (one path per line)")
    ap.add_argument("logs", nargs="+", type=Path)
    args = ap.parse_args()
    root = args.root.resolve()
    unknown = root / "_unknown"
    requested = set()
    for log in args.logs:
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            match = PATTERN.search(line)
            if match:
                requested.add("/" + match.group(1).replace("\\", "/").lower())
    moved, still_missing = [], []
    for name in sorted(requested):
        source = unknown / f"{path_hash(name):08x}"
        if source.is_file():
            target = (root / name.lstrip("/")).resolve()
            if root not in target.parents:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            source.replace(target)
            moved.append(name)
        elif not (root / name.lstrip("/")).exists():
            still_missing.append(name)
    if args.dict and moved:
        known = set(args.dict.read_text(encoding="utf-8-sig").lower().split())
        with args.dict.open("a", encoding="utf-8") as handle:
            for name in moved:
                if name not in known:
                    handle.write(name + "\n")
    print(f"requested={len(requested)} moved={len(moved)} still_missing={len(still_missing)}")
    for name in still_missing[:20]:
        print("  missing", name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
