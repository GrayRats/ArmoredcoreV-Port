#!/usr/bin/env python3
"""Extract supported BND3 containers from a directory of already-decoded .bnd3 files.

Counterpart of bnd3_batch.py for containers that were DCX-wrapped in dvdbnd (decoded by
bhd5_extract_all.py). Same strict zlib checks; unsupported format variants are listed,
not guessed. Output stays inside the workspace, one subdirectory per container.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bnd3_x360 import parse as parse_bnd3  # noqa: E402


def safe_basename(name: str) -> str:
    base = name.replace("\\", "/").rsplit("/", 1)[-1]
    base = re.sub(r"[^A-Za-z0-9._-]+", "_", base).strip("._") or "unnamed.bin"
    return base[:180]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", type=Path, help="directory with decoded *.bnd3 files")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--glob", default="*.bnd3", help="pattern under source; files without BND3 magic are skipped")
    args = ap.parse_args()
    workspace = Path(__file__).resolve().parents[1]
    source_dir, out_dir = args.source.resolve(), args.out.resolve()
    if not source_dir.is_relative_to(workspace) or not out_dir.is_relative_to(workspace):
        raise SystemExit("source and output must stay inside workspace")
    out_dir.mkdir(parents=True, exist_ok=True)
    containers = file_count = compressed = decoded_bytes = skipped = 0
    with (out_dir / "files.tsv").open("w", newline="", encoding="utf-8") as mf, \
         (out_dir / "unsupported-containers.tsv").open("w", newline="", encoding="utf-8") as uf:
        writer = csv.DictWriter(mf, delimiter="\t", fieldnames=[
            "container_hash", "index", "id", "name", "stored_size", "unpacked_size", "flags",
            "compressed", "sha256", "file"])
        writer.writeheader()
        unsupported = csv.writer(uf, delimiter="\t")
        unsupported.writerow(["container_hash", "size", "format_byte", "reason"])
        paths = sorted(p for p in source_dir.glob(args.glob) if p.is_file())
        for path in paths:
            blob = path.read_bytes()
            if blob[:4] != b"BND3":
                skipped += 1
                continue
            name = str(path.relative_to(source_dir).with_suffix("")).replace("/", "__")
            try:
                raw, records = parse_bnd3(blob)
            except (ValueError, struct.error) as ex:
                unsupported.writerow([name, len(blob), f"0x{blob[12]:02x}" if len(blob) > 12 else "", str(ex)])
                continue
            # Decode the whole container before writing anything: a corrupt stream makes the
            # container unsupported (listed with its reason) instead of aborting the run.
            decoded_records: list[tuple[dict, bytes]] = []
            try:
                for record in records:
                    size, offset = int(record["stored_size"]), int(record["offset"])
                    data = raw[offset:offset + size]
                    if record["compressed"]:
                        declared = int(record["unpacked_size"])
                        if declared > 0x20000000:
                            raise ValueError(f"unreasonable declared size in entry {record['index']}")
                        decoder = zlib.decompressobj()
                        decoded = decoder.decompress(data, declared + 1)
                        if len(decoded) > declared or decoder.unconsumed_tail or not decoder.eof:
                            raise ValueError(f"invalid zlib stream in entry {record['index']}")
                        decoded += decoder.flush()
                        if len(decoded) != declared:
                            raise ValueError(f"zlib size mismatch in entry {record['index']}")
                        data = decoded
                        compressed += 1
                    decoded_records.append((record, data))
            except (ValueError, zlib.error) as ex:
                unsupported.writerow([name, len(blob), f"0x{blob[12]:02x}", f"decode failed: {ex}"])
                continue
            containers += 1
            container_dir = out_dir / name
            container_dir.mkdir(exist_ok=True)
            for record, data in decoded_records:
                filename = f"{int(record['index']):04d}_{safe_basename(str(record['name']))}"
                (container_dir / filename).write_bytes(data)
                writer.writerow({
                    "container_hash": name, "index": record["index"], "id": record["id"],
                    "name": record["name"], "stored_size": record["stored_size"], "unpacked_size": len(data),
                    "flags": record["flags"], "compressed": record["compressed"],
                    "sha256": hashlib.sha256(data).hexdigest(), "file": f"{name}/{filename}"})
                file_count += 1
                decoded_bytes += len(data)
    print(f"containers parsed: {containers}/{len(paths) - skipped} (skipped without BND3 magic: {skipped}); files: {file_count}; zlib-decoded: {compressed}; bytes: {decoded_bytes}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
