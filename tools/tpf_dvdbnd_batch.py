#!/usr/bin/env python3
"""Extract validated Xbox 360 TPF raw texture payloads from the ACV BDT."""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import decode_dcx_dflt, parse as parse_bhd5
from tpf_x360 import parse as parse_tpf


def safe_name(name: str) -> str:
    value = re.sub(r"[^A-Za-z0-9._-]+", "_", name).strip("._") or "unnamed"
    return value[:120]


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
            raise ValueError("output must stay inside workspace")
        out_dir.mkdir(parents=True, exist_ok=True)
        manifest = out_dir / "textures.tsv"
        unsupported = out_dir / "unsupported-tpf.tsv"
        for target in (manifest, unsupported):
            if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")

        entries = parse_bhd5(args.bhd, args.bdt)
        fields = ["path_hash", "source_kind", "texture_index", "texture_name", "format", "width", "height", "mipmaps", "cubemap", "flags", "payload_size", "sha256", "file"]
        tpf_count = texture_count = decoded_count = output_bytes = 0
        with args.bdt.open("rb") as source, \
             manifest.open("w", newline="", encoding="utf-8") as mf, \
             unsupported.open("w", newline="", encoding="utf-8") as uf:
            writer = csv.DictWriter(mf, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            bad_writer = csv.writer(uf, delimiter="\t")
            bad_writer.writerow(["path_hash", "source_kind", "reason"])
            for entry in entries:
                sig = str(entry["signature"])
                if not (sig.startswith("54504600") or sig.startswith("44435800")):
                    continue
                source.seek(int(entry["offset"]))
                blob = source.read(int(entry["size"]))
                if len(blob) != int(entry["size"]):
                    raise ValueError(f'short BDT read for hash {int(entry["path_hash"]):08x}')
                kind = "direct-tpf" if sig.startswith("54504600") else "dcx-dfltpayload"
                if kind == "dcx-dfltpayload":
                    blob = decode_dcx_dflt(blob)
                    if blob[:4] != b"TPF\0":
                        continue
                    decoded_count += 1
                try:
                    tpf, textures = parse_tpf(blob)
                except ValueError as ex:
                    bad_writer.writerow([f'{int(entry["path_hash"]):08x}', kind, str(ex)])
                    continue
                tpf_count += 1
                path_hash = int(entry["path_hash"])
                for texture in textures:
                    idx = int(texture["index"])
                    filename = f'{path_hash:08x}_{idx:04d}_{safe_name(str(texture["name"]))}.x360tex'
                    target = out_dir / filename
                    if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                        raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")
                    begin = int(texture["offset"])
                    payload = tpf[begin:begin + int(texture["size"])]
                    target.write_bytes(payload)
                    writer.writerow({
                        "path_hash": f"{path_hash:08x}", "source_kind": kind,
                        "texture_index": idx, "texture_name": texture["name"],
                        "format": texture["format"], "width": texture["width"],
                        "height": texture["height"], "mipmaps": texture["mipmaps"],
                        "cubemap": texture["cubemap"], "flags": texture["flags"],
                        "payload_size": len(payload), "sha256": hashlib.sha256(payload).hexdigest(),
                        "file": filename,
                    })
                    texture_count += 1
                    output_bytes += len(payload)
        print(f"validated TPF containers: {tpf_count}; DCX-wrapped TPFs: {decoded_count}")
        print(f"raw texture payloads: {texture_count}; bytes: {output_bytes}")
        print(f"manifests: {manifest}; {unsupported}")
        return 0
    except (OSError, ValueError, KeyError) as ex:
        print(f"tpf_dvdbnd_batch: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
