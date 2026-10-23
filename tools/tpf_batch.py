#!/usr/bin/env python3
"""Extract raw texture payloads from Xbox 360 TPF files in a BND3 batch output."""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tpf_x360 import parse as parse_tpf


def safe_name(name: str) -> str:
    value = re.sub(r"[^A-Za-z0-9._-]+", "_", name).strip("._") or "unnamed"
    return value[:120]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input_dir", type=Path, help="directory produced by bnd3_batch.py")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        input_dir = args.input_dir.resolve()
        out_dir = args.out.resolve()
        if not input_dir.is_relative_to(workspace) or not out_dir.is_relative_to(workspace):
            raise ValueError("input and output directories must stay inside workspace")
        source_manifest = input_dir / "files.tsv"
        if not source_manifest.is_file():
            raise ValueError(f"missing BND3 file manifest: {source_manifest}")
        out_dir.mkdir(parents=True, exist_ok=True)
        manifest = out_dir / "textures.tsv"
        unsupported = out_dir / "unsupported-tpf.tsv"
        for target in (manifest, unsupported):
            if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")
        fields = ["container_hash", "bnd_index", "source_name", "texture_index", "texture_name", "format", "width", "height", "mipmaps", "cubemap", "flags", "payload_size", "sha256", "file"]
        total_tpf = total_textures = 0
        with source_manifest.open(newline="", encoding="utf-8") as src, \
             manifest.open("w", newline="", encoding="utf-8") as mf, \
             unsupported.open("w", newline="", encoding="utf-8") as uf:
            writer = csv.DictWriter(mf, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            bad_writer = csv.writer(uf, delimiter="\t")
            bad_writer.writerow(["container_hash", "bnd_index", "source_name", "reason"])
            for row in csv.DictReader(src, delimiter="\t"):
                if Path(row["name"].replace("\\", "/")).suffix.lower() != ".tpf":
                    continue
                source = input_dir / row["file"]
                try:
                    raw, entries = parse_tpf(source)
                except ValueError as ex:
                    bad_writer.writerow([row["container_hash"], row["index"], row["name"], str(ex)])
                    continue
                total_tpf += 1
                for tex in entries:
                    filename = (
                        f'{row["container_hash"]}_{int(row["index"]):04d}_'
                        f'{int(tex["index"]):04d}_{safe_name(str(tex["name"]))}.x360tex'
                    )
                    target = out_dir / filename
                    if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                        raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")
                    start = int(tex["offset"])
                    payload = raw[start:start + int(tex["size"])]
                    target.write_bytes(payload)
                    writer.writerow({
                        "container_hash": row["container_hash"], "bnd_index": row["index"],
                        "source_name": row["name"], "texture_index": tex["index"],
                        "texture_name": tex["name"], "format": tex["format"],
                        "width": tex["width"], "height": tex["height"], "mipmaps": tex["mipmaps"],
                        "cubemap": tex["cubemap"], "flags": tex["flags"],
                        "payload_size": len(payload), "sha256": hashlib.sha256(payload).hexdigest(),
                        "file": filename,
                    })
                    total_textures += 1
        print(f"validated/exported TPF containers: {total_tpf}; raw textures: {total_textures}")
        print(f"manifests: {manifest}; {unsupported}")
        return 0
    except (OSError, ValueError, KeyError) as ex:
        print(f"tpf_batch: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
