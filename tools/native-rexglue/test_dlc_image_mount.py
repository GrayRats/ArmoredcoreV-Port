#!/usr/bin/env python3
"""Verify the allowlisted dlc_image VFS package against the retail BHD5 and the sharded dvdbnd.bdt.

Usage: test_dlc_image_mount.py BHD SHARD_DIR   (e.g. bind/dvdbnd5.bhd and bind/shards of the game data)
"""
import hashlib, json, subprocess, sys
from pathlib import Path
root = Path(__file__).resolve().parents[2]
if len(sys.argv) != 3:
    raise SystemExit(__doc__)
bhd, shards = Path(sys.argv[1]), Path(sys.argv[2])
out = root / "tmp/acv-dlc-image-test"
subprocess.run(["python3", "-I", str(root / "tools/build_dlc_image_dir.py"), str(bhd),
                "--shards", str(shards), "--out", str(out)], check=True, stdout=subprocess.DEVNULL)
d = json.loads((out / "manifest.json").read_text())
EXPECTED = {
    "emblem/im9000.tpf", "emblem/im9001.tpf", "emblem_piece/thumbnail/piece00.tpf.dcx",
    "emblem_piece/thumbnail/piece01.tpf.dcx", "EneThumbnail/e1020.tpf", "EneThumbnail/e2020.tpf",
    "EneThumbnail/e0240.tpf", "EneThumbnail/e2030.tpf", "mapthumbnail/briefingmap00.tpf",
    "mapthumbnail/briefingmap07.tpf"}
assert {f["path"] for f in d["files"]} == EXPECTED
for f in d["files"]:
    raw = (out / f["path"]).read_bytes()
    assert len(raw) == f["bytes"]
    assert hashlib.sha256(raw).hexdigest() == f["sha256"]
    assert raw[:4] in (b"TPF\0", b"DCX\0"), f["path"]
assert sorted(str(p.relative_to(out)) for p in out.rglob("*") if p.is_file()) == sorted(EXPECTED | {"manifest.json"})
print(f"PASS: {len(EXPECTED)} requested dlc_image paths map to hash-verified retail containers (DCX wrappers kept)")
