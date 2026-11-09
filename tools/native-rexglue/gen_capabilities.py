#!/usr/bin/env python3
"""Write acv-capabilities.json next to the staged runtime binaries (run when a package is assembled).

Usage: gen_capabilities.py BIN_DIR   (for example PACKAGE/bin/game)
The launcher trusts the file only while the binaries still match it; otherwise it falls back to scanning them.
"""
import argparse, json, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_settings as S

ap = argparse.ArgumentParser()
ap.add_argument("directory", type=Path)
a = ap.parse_args()
doc = S.write_capabilities(a.directory.resolve())
print(json.dumps({"file": str(a.directory.resolve() / S.CAPABILITIES_FILE), "binaries": sorted(doc["files"]),
                  "features": doc["features"]}, ensure_ascii=False))
