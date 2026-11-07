#!/usr/bin/env python3
"""Recompute size/sha256 of every file listed in a package manifest, add extra files, drop missing ones.

Usage: refresh_package_manifest.py PACKAGE [--add REL ...] [--version X --release-name NAME --patch-level TEXT]
Game data (data/game) is verified separately by acv_install and is not listed here.
"""
import argparse, datetime, hashlib, json
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("package", type=Path)
ap.add_argument("--add", nargs="*", default=[])
ap.add_argument("--version"); ap.add_argument("--release-name"); ap.add_argument("--patch-level")
a = ap.parse_args()
p = a.package.resolve()
manifest_path = p / "manifest.json"
m = json.loads(manifest_path.read_text())
files = m["files"]
def record(path):
    data = path.read_bytes()
    return {"sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}
dropped = [n for n in files if not (p / n).is_file()]
for n in dropped: del files[n]
for n in a.add: files[n] = record(p / n)
changed = 0
for n in list(files):
    r = record(p / n)
    if files[n] != r: files[n] = r; changed += 1
for lib in list(m.get("runtime_hashes", {})):
    f = p / "bin/game" / lib
    if f.is_file(): m["runtime_hashes"][lib] = files.get("bin/game/" + lib, record(f))["sha256"]
if a.version: m["version"] = a.version
if a.release_name: m["release_name"] = a.release_name
if a.patch_level: m["patch_level"] = a.patch_level
m["updated_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
manifest_path.write_text(json.dumps(m, indent=2, ensure_ascii=False) + "\n")
print(json.dumps({"files": len(files), "changed": changed, "dropped": dropped, "added": a.add}))
