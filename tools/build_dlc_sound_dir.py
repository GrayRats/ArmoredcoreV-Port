#!/usr/bin/env python3
"""Build tmp/acv-dlc-sound: real file names for the game's `dlc_sound:\\` requests, taken from dvdbnd.

The game opens sound banks (FEV/FSB) through `dlc_sound:\\NAME`. The same files are stored in
dvdbnd.bdt under a 32-bit path hash: h = h*37 + c over the lower-cased path "/sound/NAME" (checked
against the BHD5 index: the hashed entries carry FEV1/FSB4 magic). Names are collected from resource
traces of earlier runs (open events with status 0xC000000F on dlc_sound:). Unmatched names are listed
in the manifest; nothing is guessed. Files are hard-linked (copied if linking is impossible).
"""

from __future__ import annotations

import csv
import glob
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tmp/acv-dlc-sound"
EXTRACTED = ROOT / "assets/original/acv-dvdbnd-all"
MISSING_STATUS = 0xC000000F
VOICE_LANG_RE = re.compile(r"^\s*\$\(VoiceLang\)\s*=\s*([A-Za-z0-9_-]+)", re.IGNORECASE | re.MULTILINE)


def path_hash(path: str) -> int:
    value = 0
    for byte in path.lower().encode("latin1"):
        value = (value * 37 + byte) & 0xFFFFFFFF
    return value


def requested_names() -> set[str]:
    names: set[str] = set()
    for trace in glob.glob(str(ROOT / "logs/**/resources*.jsonl"), recursive=True):
        with open(trace, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if '"event":"open"' not in line:
                    continue
                try:
                    row = json.loads(line)
                except ValueError:
                    continue
                if row.get("status") != MISSING_STATUS or not row.get("path_hex"):
                    continue
                text = bytes.fromhex(row["path_hex"]).decode("latin1")
                if text.lower().startswith("dlc_sound:\\"):
                    names.add(text.split("\\", 1)[1])
    return names


def configured_voice_language() -> str | None:
    """Read the voice-language token from the extracted retail ACV config."""
    for path in (ROOT / "assets/original/acv-bnd3-all").glob("*/0000_acv.ini"):
        match = VOICE_LANG_RE.search(path.read_text(encoding="latin1", errors="replace"))
        if match:
            return match.group(1)
    return None


def resolve_voice_language(name: str, voice_language: str | None) -> str | None:
    """Return the concrete bank name, or None if its voice-language token is unresolved."""
    if not re.search(r"\$\(\s*voicelang\s*\)", name, re.IGNORECASE):
        return name
    if not voice_language:
        return None
    return re.sub(r"\$\(\s*voicelang\s*\)", voice_language, name, flags=re.IGNORECASE)


MAGIC = {".fev": b"FEV1", ".fsb": b"FSB4"}


def candidates_from_fev(index: dict) -> set[str]:
    """Names built from strings inside the extracted FEV1 files; kept only if the path hash exists."""
    found: set[str] = set()
    for row in index.values():
        if not row["file"].endswith(".fev"):
            continue
        data = (EXTRACTED / row["file"]).read_bytes()
        for match in re.finditer(rb"[A-Za-z0-9_\-\.]{3,60}", data):
            text = match.group().decode("latin1")
            for name in (text + ".fsb", text + ".fev", "acv_" + text + ".fev", "acv_" + text + ".fsb", text):
                if path_hash("/sound/" + name) in index:
                    found.add(name)
    return found


def main() -> int:
    index = {}
    with (ROOT / "assets/manifests/acv-dvdbnd-all.tsv").open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            if row["file"]:
                index[int(row["path_hash"], 16)] = row
    OUT.mkdir(parents=True, exist_ok=True)
    matched, unmatched = [], []
    requested = requested_names()
    voice_language = configured_voice_language()
    derived = candidates_from_fev(index)
    for name in sorted(requested | derived):
        if "\\" in name or "/" in name or name in {".", ".."}:
            unmatched.append({"name": name, "reason": "subpath in name"})
            continue
        resolved_name = resolve_voice_language(name, voice_language)
        if resolved_name is None:
            unmatched.append({"name": name, "reason": "VoiceLang is not defined in extracted acv.ini"})
            continue
        if re.search(r"\$\(", resolved_name):
            unmatched.append({"name": name, "resolved_name": resolved_name,
                              "reason": "unresolved variable in sound bank name"})
            continue
        row = index.get(path_hash("/sound/" + resolved_name))
        if row is None:
            unmatched.append({"name": name, "resolved_name": resolved_name,
                              "reason": "hash of resolved /sound/NAME not in dvdbnd index"})
            continue
        source = EXTRACTED / row["file"]
        extension = os.path.splitext(resolved_name)[1].lower()
        if extension in MAGIC and source.read_bytes()[:4] != MAGIC[extension]:
            unmatched.append({"name": name, "resolved_name": resolved_name,
                              "reason": "hash match but magic differs from extension"})
            continue
        # Keep a literal-request alias when the guest passes an unexpanded
        # $(VoiceLang) token to the VFS. On disk this exact filename lets the
        # request resolve to the verified locale-specific archive payload.
        target = OUT / name
        if target.exists() or target.is_symlink():
            target.unlink()
        try:
            os.link(source, target)
        except OSError:
            shutil.copy2(source, target)
        data = target.read_bytes()
        matched.append({"name": name, "resolved_name": resolved_name,
                        "origin": "requested" if name in requested else "derived-from-fev", "source": row["file"], "bytes": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(), "magic": data[:4].hex()})
    (OUT / "manifest.json").write_text(json.dumps({"voice_language": voice_language,
                                                    "matched": matched, "unmatched": unmatched}, indent=1) + "\n")
    print(f"matched {len(matched)}, unmatched {len(unmatched)}")
    for item in unmatched:
        print("unmatched:", item["name"], "-", item["reason"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
