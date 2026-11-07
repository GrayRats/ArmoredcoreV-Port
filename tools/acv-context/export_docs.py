#!/usr/bin/env python3
"""Export knowledge.sqlite3 (documents and attachments) as plain files into a docs directory.

Documents keep their legacy layout: tools/acv-context/docs/<x> -> <out>/<x>; legacy files that lived in the
repository root go to <out>/project/<name>. Every document body and attachment is checked against its stored sha256
before it is written. The database is opened read-only. Usage: export_docs.py OUT_DIR [--db PATH]
"""
import argparse, hashlib, sqlite3, sys
from pathlib import Path

PREFIX = "tools/acv-context/docs/"

def target(legacy, out):
    if legacy.startswith(PREFIX):
        relative = Path(legacy[len(PREFIX):])
    elif "/" not in legacy:
        relative = Path("project") / legacy
    else:
        raise SystemExit(f"unexpected legacy path: {legacy}")
    if relative.is_absolute() or ".." in relative.parts:
        raise SystemExit(f"unsafe path: {legacy}")
    return out / relative

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--db", type=Path, default=Path(__file__).with_name("knowledge.sqlite3"))
    a = ap.parse_args()
    db = sqlite3.connect(f"file:{a.db}?mode=ro", uri=True)
    written = []
    for slug, legacy, body, sha in db.execute("select slug, legacy_path, body, sha256 from doc order by legacy_path"):
        data = body.encode("utf-8")
        if hashlib.sha256(data).hexdigest() != sha:
            raise SystemExit(f"{slug}: body does not match its sha256")
        path = target(legacy, a.out)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        written.append(path)
    for name, legacy, data, sha in db.execute("select name, legacy_path, data, sha256 from attachment order by legacy_path"):
        if hashlib.sha256(data).hexdigest() != sha:
            raise SystemExit(f"{name}: attachment does not match its sha256")
        path = target(legacy, a.out)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        written.append(path)
    print(f"exported {len(written)} files to {a.out}")

if __name__ == "__main__":
    sys.exit(main())
