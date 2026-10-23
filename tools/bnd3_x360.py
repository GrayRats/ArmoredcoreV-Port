#!/usr/bin/env python3
"""Validate/extract the observed big-endian, 32-bit-offset BND3 variant."""

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
    if len(raw) < 32 or raw[:4] != b"BND3":
        raise ValueError("not a BND3 container")
    version = raw[4:12].split(b"\0", 1)[0].decode("ascii", "strict")
    fmt = raw[12]
    big_endian, bit_big_endian, reserved = raw[13:16]
    if fmt not in (0x26, 0x2C) or big_endian != 1 or bit_big_endian != 1 or reserved != 0:
        raise ValueError("unsupported BND3 format; expected format 0x26 or 0x2c, big-endian flags")
    # 0x26 records carry a file id (24 bytes); 0x2c records do not (20 bytes: flags, stored
    # size, data offset, name offset, unpacked size). Same strict bounds checks for both.
    record_size = 24 if fmt == 0x26 else 20
    count, headers_end, unknown, zero = struct.unpack_from(">IIII", raw, 16)
    if count > 100_000 or headers_end > len(raw) or unknown not in (0, 0x80000000) or zero != 0:
        raise ValueError("invalid BND3 header fields")
    table_end = 32 + count * record_size
    if table_end > headers_end:
        raise ValueError("BND3 file table exceeds declared headers end")

    entries: list[dict[str, int | str]] = []
    data_ranges: list[tuple[int, int]] = []
    for i in range(count):
        at = 32 + i * record_size
        flags = raw[at]
        if raw[at + 1:at + 4] != b"\0\0\0":
            raise ValueError(f"file {i} has unexpected reserved bytes")
        if fmt == 0x26:
            stored_size, offset, file_id, name_offset, unpacked_size = struct.unpack_from(">iI iI i", raw, at + 4)
        else:
            stored_size, offset, name_offset, unpacked_size = struct.unpack_from(">iI I i", raw, at + 4)
            file_id = ""
        if stored_size < 0 or unpacked_size < 0 or offset > len(raw) or stored_size > len(raw) - offset:
            raise ValueError(f"file {i} data range is outside BND3 bounds")
        if name_offset < table_end or name_offset >= headers_end:
            raise ValueError(f"file {i} name pointer is outside BND3 name area")
        end = raw.find(b"\0", name_offset, headers_end)
        if end < 0:
            raise ValueError(f"file {i} name is not terminated before headers end")
        try:
            name = raw[name_offset:end].decode("shift_jis")
        except UnicodeDecodeError as ex:
            raise ValueError(f"file {i} name is not valid Shift-JIS") from ex
        compressed = bool(flags & 1)
        if not compressed and stored_size != unpacked_size:
            raise ValueError(f"file {i} is uncompressed but stored/uncompressed sizes differ")
        entries.append({
            "index": i, "id": file_id, "name": name, "offset": offset,
            "stored_size": stored_size, "unpacked_size": unpacked_size,
            "flags": flags, "compressed": int(compressed),
        })
        data_ranges.append((offset, offset + stored_size))
    for previous, current in zip(sorted(data_ranges), sorted(data_ranges)[1:]):
        if current[0] < previous[1]:
            raise ValueError("BND3 file-data ranges overlap")
    return raw, entries


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bnd3", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--prefix", default="")
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        out_dir = args.out.resolve()
        if not out_dir.is_relative_to(workspace):
            raise ValueError(f"output must stay inside workspace: {args.out}")
        raw, entries = parse(args.bnd3)
        out_dir.mkdir(parents=True, exist_ok=True)
        manifest = out_dir / "files.tsv"
        if manifest.is_symlink() or not manifest.resolve().is_relative_to(workspace):
            raise ValueError("manifest output must stay inside workspace and cannot be a symlink")
        fields = ["index", "id", "name", "offset", "stored_size", "unpacked_size", "flags", "compressed", "sha256", "file"]
        with manifest.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            for e in entries:
                base = re.sub(r"[^A-Za-z0-9._-]+", "_", Path(str(e["name"])).name).strip("._") or "unnamed"
                filename = f'{args.prefix}{int(e["index"]):04d}_{base}.bin'
                target = out_dir / filename
                if target.is_symlink() or not target.resolve().is_relative_to(workspace):
                    raise ValueError(f"output must stay inside workspace and cannot be a symlink: {target}")
                data = raw[int(e["offset"]):int(e["offset"]) + int(e["stored_size"])]
                target.write_bytes(data)
                writer.writerow({
                    "index": e["index"], "id": e["id"], "name": e["name"], "offset": e["offset"],
                    "stored_size": len(data), "unpacked_size": e["unpacked_size"], "flags": e["flags"],
                    "compressed": e["compressed"], "sha256": hashlib.sha256(data).hexdigest(), "file": filename,
                })
                print(f'{e["name"]}: {len(data)} bytes; compressed={e["compressed"]}')
        print(f"Validated {len(entries)} files in BND3 {raw[4:12]!r}; manifest: {manifest}")
        return 0
    except (OSError, ValueError, UnicodeDecodeError, struct.error) as ex:
        print(f"bnd3_x360: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
