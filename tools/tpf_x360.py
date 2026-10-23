#!/usr/bin/env python3
"""Validate and extract raw texture payloads from the observed Xbox 360 TPF form."""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
import struct
import sys
from pathlib import Path


def parse(source: Path | bytes) -> tuple[bytes, list[dict[str, int | str]]]:
    raw = source.read_bytes() if isinstance(source, Path) else source
    if len(raw) < 16 or raw[:4] != b"TPF\0":
        raise ValueError("not a TPF container")
    data_size, count = struct.unpack_from(">II", raw, 4)
    platform, flags, encoding, reserved = raw[12:16]
    if platform != 1 or reserved != 0:
        raise ValueError(f"unsupported TPF platform/reserved bytes: {platform}/{reserved}")
    if encoding not in (0, 1, 2) or count > 100_000:
        raise ValueError("unsupported encoding or implausible texture count")
    table_end = 16 + count * 28
    if table_end > len(raw):
        raise ValueError("texture table exceeds file bounds")

    entries: list[dict[str, int | str]] = []
    total_payload = 0
    ranges: list[tuple[int, int]] = []
    for i in range(count):
        at = 16 + i * 28
        offset, size = struct.unpack_from(">Ii", raw, at)
        fmt, cubemap, mipmaps, entry_flags = raw[at + 8:at + 12]
        width, height = struct.unpack_from(">HH", raw, at + 12)
        reserved1, name_offset, reserved2 = struct.unpack_from(">III", raw, at + 16)
        if size < 0 or offset > len(raw) or size > len(raw) - offset:
            raise ValueError(f"texture {i} payload is outside TPF bounds")
        if reserved1 != 0 or reserved2 != 0:
            raise ValueError(f"texture {i} has unexpected reserved fields")
        if name_offset < table_end or name_offset >= len(raw):
            raise ValueError(f"texture {i} name offset is outside the name area")
        end = raw.find(b"\0\0" if encoding == 1 else b"\0", name_offset)
        if end < 0:
            raise ValueError(f"texture {i} name is not terminated")
        name_bytes = raw[name_offset:end]
        codec = "utf-16-be" if encoding == 1 else "shift_jis"
        try:
            name = name_bytes.decode(codec)
        except UnicodeDecodeError as ex:
            raise ValueError(f"texture {i} name cannot be decoded as {codec}") from ex
        entries.append({
            "index": i, "name": name, "offset": offset, "size": size,
            "format": fmt, "cubemap": cubemap, "mipmaps": mipmaps,
            "flags": entry_flags, "width": width, "height": height,
        })
        total_payload += size
        ranges.append((offset, offset + size))

    if total_payload != data_size:
        raise ValueError(f"header payload sum {data_size} != entry payload sum {total_payload}")
    for previous, current in zip(sorted(ranges), sorted(ranges)[1:]):
        if current[0] < previous[1]:
            raise ValueError("texture payload ranges overlap")
    return raw, entries


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("tpf", type=Path)
    ap.add_argument("--out", required=True, type=Path, help="workspace-local output directory")
    ap.add_argument("--prefix", default="", help="stable source hash/name prefix")
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        out_dir = args.out.resolve()
        if not out_dir.is_relative_to(workspace):
            raise ValueError(f"output must stay inside workspace: {args.out}")
        raw, entries = parse(args.tpf)
        out_dir.mkdir(parents=True, exist_ok=True)
        manifest_path = out_dir / "textures.tsv"
        if manifest_path.is_symlink() or not manifest_path.resolve().is_relative_to(workspace):
            raise ValueError("manifest output must stay inside workspace and cannot be a symlink")
        fields = ["index", "name", "format", "width", "height", "mipmaps", "cubemap", "flags", "payload_size", "sha256", "file"]
        with manifest_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            for entry in entries:
                base = re.sub(r"[^A-Za-z0-9._-]+", "_", str(entry["name"])).strip("._") or "unnamed"
                filename = f'{args.prefix}{int(entry["index"]):04d}_{base}.x360tex'
                payload_path = out_dir / filename
                if payload_path.is_symlink() or not payload_path.resolve().is_relative_to(workspace):
                    raise ValueError(f"texture output must stay inside workspace and cannot be a symlink: {payload_path}")
                blob = raw[int(entry["offset"]):int(entry["offset"]) + int(entry["size"])]
                payload_path.write_bytes(blob)
                writer.writerow({
                    "index": entry["index"], "name": entry["name"], "format": entry["format"],
                    "width": entry["width"], "height": entry["height"], "mipmaps": entry["mipmaps"],
                    "cubemap": entry["cubemap"], "flags": entry["flags"], "payload_size": len(blob),
                    "sha256": hashlib.sha256(blob).hexdigest(), "file": filename,
                })
                print(f'{filename}: {len(blob)} bytes; {entry["width"]}x{entry["height"]}; format={entry["format"]}')
        print(f"Validated {len(entries)} textures; manifest: {manifest_path}")
        return 0
    except (OSError, ValueError, struct.error) as ex:
        print(f"tpf_x360: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
