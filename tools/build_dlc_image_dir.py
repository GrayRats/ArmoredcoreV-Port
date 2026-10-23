#!/usr/bin/env python3
"""Build a minimal dlc_image VFS tree from verified ACV dvdbnd entries."""
from __future__ import annotations
import argparse, hashlib, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import decode_dcx_dflt, parse as parse_bhd5
from tpf_x360 import parse as parse_tpf

# The names and hashes are confirmed by runtime requests and the extracted TPF manifest.
# Preserve the original BDT bytes: piece00 is DCX-wrapped in the retail archive.
FILES = {
    "emblem/im9000.tpf": (0x6B993E99, "im9000"),
    "emblem/im9001.tpf": (0x6BB5D78A, "im9001"),
    "emblem_piece/thumbnail/piece00.tpf.dcx": (0x92039491, "piece00"),
    "emblem_piece/thumbnail/piece01.tpf.dcx": (0x621AA772, "piece01"),
}

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    workspace = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    if not out.is_relative_to(workspace) or out == workspace or out.is_symlink():
        raise SystemExit("output must be a non-symlink directory inside the workspace")
    entries = parse_bhd5(args.bhd, args.bdt)
    by_hash = {int(e["path_hash"]): e for e in entries}
    manifest = {"source_bhd": str(args.bhd), "source_bdt": str(args.bdt), "files": []}
    out.mkdir(parents=True, exist_ok=True)
    with args.bdt.open("rb") as source:
        for relative, (path_hash, expected_name) in FILES.items():
            entry = by_hash.get(path_hash)
            if entry is None:
                raise ValueError(f"missing BHD5 path hash {path_hash:08x}")
            source.seek(int(entry["offset"]))
            original = source.read(int(entry["size"]))
            if len(original) != int(entry["size"]):
                raise ValueError(f"short BDT read for {path_hash:08x}")
            unpacked = decode_dcx_dflt(original) if original.startswith(b"DCX\0") else original
            if not unpacked.startswith(b"TPF\0"):
                raise ValueError(f"{path_hash:08x} is not a TPF payload")
            _, textures = parse_tpf(unpacked)
            names = [str(t["name"]) for t in textures]
            if expected_name not in names:
                raise ValueError(f"{path_hash:08x}: expected texture {expected_name}, got {names}")
            target = out / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_symlink() or not target.resolve().is_relative_to(out):
                raise ValueError(f"unsafe output path: {target}")
            target.write_bytes(original)
            manifest["files"].append({"path": relative, "path_hash": f"{path_hash:08x}",
                "texture_names": names, "bytes": len(original),
                "sha256": hashlib.sha256(original).hexdigest(),
                "decoded_magic": unpacked[:4].decode("ascii")})
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"out": str(out), "files": manifest["files"]}, indent=2))
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"build_dlc_image_dir: {exc}", file=sys.stderr)
        raise SystemExit(2)
