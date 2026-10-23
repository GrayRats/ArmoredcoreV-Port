#!/usr/bin/env python3
"""Verify the allowlisted emblem VFS package against the retail BHD5/BDT."""
import hashlib, json, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
out = root / "tmp/acv-dlc-image-test"
subprocess.run(["python3", str(root / "tools/build_dlc_image_dir.py"),
                str(root / "tmp/iso_root/bind/dvdbnd5.bhd"),
                str(root / "tmp/iso_root/bind/dvdbnd.bdt"), "--out", str(out)],
               check=True, stdout=subprocess.DEVNULL)
d = json.loads((out / "manifest.json").read_text())
assert len(d["files"]) == 4
for f in d["files"]:
    p = out / f["path"]
    raw = p.read_bytes()
    assert len(raw) == f["bytes"]
    assert hashlib.sha256(raw).hexdigest() == f["sha256"]
    assert f["path_hash"] in {"6b993e99", "6bb5d78a", "92039491", "621aa772"}
assert (out / "emblem/im9000.tpf").read_bytes()[:4] == b"TPF\0"
assert (out / "emblem/im9001.tpf").read_bytes()[:4] == b"TPF\0"
assert (out / "emblem_piece/thumbnail/piece00.tpf.dcx").read_bytes()[:4] == b"DCX\0"
assert (out / "emblem_piece/thumbnail/piece01.tpf.dcx").read_bytes()[:4] == b"DCX\0"
assert sorted(str(p.relative_to(out)) for p in out.rglob("*") if p.is_file()) == [
    "emblem/im9000.tpf", "emblem/im9001.tpf", "emblem_piece/thumbnail/piece00.tpf.dcx",
    "emblem_piece/thumbnail/piece01.tpf.dcx", "manifest.json"]
print("PASS: 4 requested VFS paths map to hash-verified retail BDT containers; DCX wrapper retained")
