#!/usr/bin/env python3
"""Index/extract the observed 16-byte-entry BHD5 dvdbnd variant.

This is intentionally conservative: it recognizes the variant from structural
invariants and never guesses original filenames from the 32-bit path hashes.
All outputs must be placed by the caller inside the reconstruction workspace.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path


def parse(bhd: Path, bdt: Path) -> list[dict[str, int | str]]:
    data_size = bdt.stat().st_size
    data_file = bdt.open("rb")
    raw = bhd.read_bytes()
    if raw[:4] != b"BHD5":
        raise ValueError("BHD magic mismatch")
    endian_marker = raw[4]
    if endian_marker not in (0, 0xFF):
        raise ValueError(f"unsupported endian marker {endian_marker:#x}")
    endian = ">" if endian_marker == 0 else "<"
    u32 = lambda off: struct.unpack_from(endian + "I", raw, off)[0]
    if len(raw) < 24 or u32(8) != 1:
        raise ValueError("unsupported BHD5 header variant")
    declared_size, bucket_count, buckets_offset = u32(12), u32(16), u32(20)
    if declared_size != len(raw):
        raise ValueError(f"declared BHD size {declared_size} != actual {len(raw)}")
    if bucket_count > 1_000_000 or buckets_offset + bucket_count * 8 > len(raw):
        raise ValueError("bucket table exceeds BHD bounds")

    entries: list[dict[str, int | str]] = []
    for bucket in range(bucket_count):
        count, offset = struct.unpack_from(endian + "II", raw, buckets_offset + bucket * 8)
        if count > 1_000_000 or offset + count * 16 > len(raw):
            raise ValueError(f"bucket {bucket} entry table exceeds BHD bounds")
        for item in range(count):
            pos = offset + item * 16
            path_hash, size = struct.unpack_from(endian + "Ii", raw, pos)
            data_offset = struct.unpack_from(endian + "Q", raw, pos + 8)[0]
            if size < 0 or data_offset > data_size or size > data_size - data_offset:
                raise ValueError(
                    f"invalid BDT range in bucket {bucket}, entry {item}: "
                    f"offset={data_offset:#x}, size={size}"
                )
            data_file.seek(data_offset)
            signature = data_file.read(min(8, size)).hex()
            entries.append({
                "bucket": bucket,
                "entry": item,
                "path_hash": path_hash,
                "offset": data_offset,
                "size": size,
                "signature": signature,
            })
    data_file.close()
    return entries


def decode_dcx_dflt(blob: bytes) -> bytes:
    """Decode the locally observed ACV DCX/DCP DFLT form with strict size checks."""
    if len(blob) < 0x4C or blob[:4] != b"DCX\0" or blob[0x24:0x2C] != b"DCP\0DFLT" or blob[0x44:0x48] != b"DCA\0":
        raise ValueError("not the validated DCX/DCP DFLT variant")
    raw_size, compressed_size = struct.unpack_from(">II", blob, 0x1C)
    dca_size = struct.unpack_from(">I", blob, 0x48)[0]
    if raw_size > 0x10000000 or dca_size != 8 or 0x4C + compressed_size != len(blob):
        raise ValueError("DCX payload bounds/header mismatch")
    decoder = zlib.decompressobj()
    decoded = decoder.decompress(blob[0x4C:], raw_size + 1)
    if len(decoded) > raw_size or decoder.unconsumed_tail or decoder.unused_data or not decoder.eof:
        raise ValueError("DCX stream exceeds declared size or is incomplete")
    decoded += decoder.flush()
    if len(decoded) != raw_size:
        raise ValueError(f"DCX output size {len(decoded)} != declared {raw_size}")
    return decoded


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--manifest", type=Path, help="write a TSV index")
    ap.add_argument("--extract-hash", action="append", default=[],
                    help="extract matching 32-bit hash (repeatable); no bulk extraction")
    ap.add_argument("--out", type=Path, help="required with --extract-hash")
    ap.add_argument("--decompress-dcx", action="store_true",
                    help="decode only the validated DCX/DCP DFLT zlib variant")
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        for output in (args.manifest, args.out):
            if output is not None and not output.resolve().is_relative_to(workspace):
                raise ValueError(f"output must stay inside workspace: {output}")
        entries = parse(args.bhd, args.bdt)
        if args.manifest:
            args.manifest.parent.mkdir(parents=True, exist_ok=True)
            if args.manifest.is_symlink() or not args.manifest.resolve().is_relative_to(workspace):
                raise ValueError(f"manifest target must stay inside workspace and cannot be a symlink: {args.manifest}")
            with args.manifest.open("w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=["bucket", "entry", "path_hash", "offset", "size", "signature"], delimiter="\t")
                w.writeheader()
                w.writerows(entries)
        if args.extract_hash:
            if args.out is None:
                raise ValueError("--out is required with --extract-hash")
            wanted = {int(value, 0) for value in args.extract_hash}
            matched = [e for e in entries if e["path_hash"] in wanted]
            if not matched:
                raise ValueError("no requested path hashes found")
            args.out.mkdir(parents=True, exist_ok=True)
            with args.bdt.open("rb") as src:
                for e in matched:
                    src.seek(e["offset"])
                    blob = src.read(e["size"])
                    if len(blob) != e["size"]:
                        raise ValueError("short read from BDT")
                    if args.decompress_dcx:
                        try:
                            blob = decode_dcx_dflt(blob)
                        except ValueError as ex:
                            raise ValueError(f'entry {e["path_hash"]:08x}: {ex}') from ex
                    suffix = ".decoded.bin" if args.decompress_dcx else ".bin"
                    out = args.out / f'{e["path_hash"]:08x}{suffix}'
                    if out.is_symlink() or not out.resolve().is_relative_to(workspace):
                        raise ValueError(f"extraction target must stay inside workspace and cannot be a symlink: {out}")
                    out.write_bytes(blob)
                    print(f'{out}: {len(blob)} bytes sha256={hashlib.sha256(blob).hexdigest()}')
        offsets = [e["offset"] for e in entries]
        ends = [e["offset"] + e["size"] for e in entries]
        print(json.dumps({
            "bhd": str(args.bhd), "bdt": str(args.bdt), "entry_count": len(entries),
            "total_logical_bytes": sum(e["size"] for e in entries),
            "min_offset": min(offsets, default=0), "max_end": max(ends, default=0),
            "manifest": str(args.manifest) if args.manifest else None,
        }, indent=2))
        return 0
    except (OSError, ValueError, struct.error) as ex:
        print(f"bhd5_index: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
