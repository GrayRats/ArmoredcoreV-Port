#!/usr/bin/env python3
"""Build a minimal dlc_image VFS tree from verified ACV dvdbnd entries.

The archive is read either from the original dvdbnd.bdt or, after sharding, from the shard directory
(bind/shards with shards.idx) through acv_shard_dvdbnd.ShardReader.
"""
from __future__ import annotations
import argparse, hashlib, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import decode_dcx_dflt, parse as parse_bhd5, parse_archive
from acv_shard_dvdbnd import ShardReader
from tpf_x360 import parse as parse_tpf

# Requested by the guest as dlc_image:\<relative path> and stored in the retail BHD5 under "/image/<relative path>".
# Each entry: relative path -> expected texture name inside the TPF. piece00/01 are DCX-wrapped in the archive;
# the original bytes are kept (the guest decodes them itself).
FILES = {
    "emblem/im9000.tpf": "im9000",
    "emblem/im9001.tpf": "im9001",
    "emblem_piece/thumbnail/piece00.tpf.dcx": "piece00",
    "emblem_piece/thumbnail/piece01.tpf.dcx": "piece01",
    "EneThumbnail/e1020.tpf": "e1020_t",
    "EneThumbnail/e2020.tpf": "e2020_t",
    "EneThumbnail/e0240.tpf": "e0240_t",
    "EneThumbnail/e2030.tpf": "e2030_t",
    "mapthumbnail/briefingmap00.tpf": "briefingmap00",
    "mapthumbnail/briefingmap07.tpf": "briefingmap07",
}

def path_hash(path: str) -> int:
    value = 0
    for byte in path.lower().encode("latin1"):
        value = (value * 37 + byte) & 0xFFFFFFFF
    return value

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path, nargs="?", help="original dvdbnd.bdt (omit when --shards is used)")
    ap.add_argument("--shards", type=Path, help="shard directory with shards.idx (instead of the .bdt)")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    if (args.bdt is None) == (args.shards is None):
        raise SystemExit("pass either the dvdbnd.bdt or --shards DIR")
    workspace = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if not out.is_relative_to(workspace) or out == workspace or out.is_symlink():
        raise SystemExit("output must be a non-symlink directory inside the workspace")
    if args.shards is not None:
        source = ShardReader(args.shards)
        entries = parse_archive(args.bhd, source.size, lambda: source)
        source_name = str(args.shards / "shards.idx")
    else:
        source = args.bdt.open("rb")
        entries = parse_bhd5(args.bhd, args.bdt)
        source_name = str(args.bdt)
    by_hash = {int(e["path_hash"]): e for e in entries}
    manifest = {"source_bhd": str(args.bhd), "source_archive": source_name, "files": []}
    out.mkdir(parents=True, exist_ok=True)
    try:
        for relative, expected_name in FILES.items():
            path_hash_value = path_hash("/image/" + relative)
            entry = by_hash.get(path_hash_value)
            if entry is None:
                raise ValueError(f"missing BHD5 path hash {path_hash_value:08x} for {relative}")
            source.seek(int(entry["offset"]))
            original = source.read(int(entry["size"]))
            if len(original) != int(entry["size"]):
                raise ValueError(f"short archive read for {relative}")
            unpacked = decode_dcx_dflt(original) if original.startswith(b"DCX\0") else original
            if not unpacked.startswith(b"TPF\0"):
                raise ValueError(f"{relative} is not a TPF payload")
            _, textures = parse_tpf(unpacked)
            names = [str(t["name"]) for t in textures]
            if expected_name not in names:
                raise ValueError(f"{relative}: expected texture {expected_name}, got {names}")
            target = out / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_symlink() or not target.resolve().is_relative_to(out):
                raise ValueError(f"unsafe output path: {target}")
            target.write_bytes(original)
            manifest["files"].append({"path": relative, "path_hash": f"{path_hash_value:08x}",
                "texture_names": names, "bytes": len(original),
                "sha256": hashlib.sha256(original).hexdigest(),
                "decoded_magic": unpacked[:4].decode("ascii")})
    finally:
        source.close()
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"out": str(out), "files": manifest["files"]}, indent=2))
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"build_dlc_image_dir: {exc}", file=sys.stderr)
        raise SystemExit(2)
