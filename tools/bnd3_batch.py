#!/usr/bin/env python3
"""Extract supported ACV BND3 entries from the workspace dvdbnd index."""

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
from bnd3_x360 import parse as parse_bnd3
from bhd5_index import parse as parse_bhd5


def safe_basename(name: str) -> str:
    base = name.replace("\\", "/").rsplit("/", 1)[-1]
    base = re.sub(r"[^A-Za-z0-9._-]+", "_", base).strip("._") or "unnamed.bin"
    return base[:180]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        out_dir = args.out.resolve()
        if not out_dir.is_relative_to(workspace):
            raise ValueError(f"output must stay inside workspace: {args.out}")
        out_dir.mkdir(parents=True, exist_ok=True)
        files_manifest = out_dir / "files.tsv"
        unsupported_manifest = out_dir / "unsupported-containers.tsv"
        for target in (files_manifest, unsupported_manifest):
            if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                raise ValueError(f"manifest output must stay inside workspace and cannot be a symlink: {target}")

        entries = parse_bhd5(args.bhd, args.bdt)
        candidates = [e for e in entries if e["signature"].startswith("424e4433")]
        container_count = file_count = compressed_count = decoded_bytes = 0
        errors: list[tuple[str, str]] = []
        with args.bdt.open("rb") as source, \
             files_manifest.open("w", newline="", encoding="utf-8") as mf, \
             unsupported_manifest.open("w", newline="", encoding="utf-8") as uf:
            writer = csv.DictWriter(mf, fieldnames=["container_hash", "index", "id", "name", "stored_size", "unpacked_size", "flags", "compressed", "sha256", "file"], delimiter="\t")
            writer.writeheader()
            unsupported = csv.writer(uf, delimiter="\t")
            unsupported.writerow(["container_hash", "size", "format_byte", "reason"])
            for container in candidates:
                source.seek(int(container["offset"]))
                blob = source.read(int(container["size"]))
                path_hash = int(container["path_hash"])
                try:
                    raw, records = parse_bnd3(blob)
                except ValueError as ex:
                    unsupported.writerow([f"{path_hash:08x}", len(blob), f"0x{blob[12]:02x}" if len(blob) > 12 else "", str(ex)])
                    continue
                container_count += 1
                for record in records:
                    size = int(record["stored_size"])
                    offset = int(record["offset"])
                    data = raw[offset:offset + size]
                    unpacked_size = int(record["unpacked_size"])
                    if record["compressed"]:
                        if unpacked_size > 0x20000000:
                            raise ValueError(f"unreasonable declared output size in {path_hash:08x}:{record['index']}")
                        decoder = zlib.decompressobj()
                        decoded = decoder.decompress(data, unpacked_size + 1)
                        if len(decoded) > unpacked_size or decoder.unconsumed_tail or not decoder.eof:
                            raise ValueError(f"invalid zlib stream in {path_hash:08x}:{record['index']}")
                        decoded += decoder.flush()
                        if len(decoded) != unpacked_size:
                            raise ValueError(f"zlib output size mismatch in {path_hash:08x}:{record['index']}")
                        data = decoded
                        compressed_count += 1
                    filename = f"{path_hash:08x}_{int(record['index']):04d}_{safe_basename(str(record['name']))}"
                    target = out_dir / filename
                    if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                        raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")
                    target.write_bytes(data)
                    writer.writerow({
                        "container_hash": f"{path_hash:08x}", "index": record["index"],
                        "id": record["id"], "name": record["name"], "stored_size": size,
                        "unpacked_size": len(data), "flags": record["flags"],
                        "compressed": record["compressed"], "sha256": hashlib.sha256(data).hexdigest(),
                        "file": filename,
                    })
                    file_count += 1
                    decoded_bytes += len(data)
        print(f"supported BND3 containers: {container_count}/{len(candidates)}")
        print(f"extracted files: {file_count}; zlib-decoded: {compressed_count}; output bytes: {decoded_bytes}")
        print(f"manifests: {files_manifest}; {unsupported_manifest}")
        return 0
    except (OSError, ValueError, struct.error, zlib.error) as ex:
        print(f"bnd3_batch: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
