#!/usr/bin/env python3
"""Split the ACV dvdbnd.bdt archive into several shard files plus an index.

The original archive and its BHD5 header are opened read-only. Every BHD5 entry is copied byte for byte
(DCX stays compressed) into a shard chosen by its logical area (map, model group, sound, language, ...),
entries inside a shard keep the original archive order and start on a 2048-byte boundary. index.json maps
(original offset, size) as seen in the BHD5 to (shard, offset, size), so a loader can serve the guest's
unchanged reads from small shards instead of one 3 GB file.

Names come from a manifest written by acv_loose_unpack.py (path, size, sha256); an entry is matched by
(path hash, sha256). Entries without a name go to the "unknown" shard.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

ALIGN = 2048
INDEX_VERSION = 1
IDX_MAGIC = b"ACVSHD01"


def load_parse():
    spec = importlib.util.spec_from_file_location("bhd5_index", Path(__file__).with_name("bhd5_index.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.parse


def path_hash(path: str) -> int:
    value = 0
    for char in path:
        value = (value * 37 + ord(char)) & 0xFFFFFFFF
    return value


def group_of(path: str | None) -> str:
    """Logical shard group of an archive-relative path (None = unnamed entry)."""
    if path and path.startswith("_gap/"):
        return "gaps"
    if not path or path.startswith("_unknown/"):
        return "unknown"
    parts = path.split("/")
    top = parts[0]
    if top == "model":
        if len(parts) >= 3 and parts[1] == "map" and re.fullmatch(r"m\d{4}", parts[2]):
            return f"map-{parts[2]}"
        return f"model-{parts[1]}" if len(parts) > 2 else "model-root"
    if top == "lang" and len(parts) > 2:
        return f"lang-{parts[1]}"
    if top in ("sound", "movie", "image"):
        return top
    return "core"


def plan(entries: list[dict], max_bytes: int) -> dict[str, list[dict]]:
    """Assign entries to shard names; a group larger than max_bytes is split into <group>-<n> in archive order."""
    groups: dict[str, list[dict]] = {}
    for entry in sorted(entries, key=lambda item: item["offset"]):
        groups.setdefault(group_of(entry.get("path")), []).append(entry)
    shards: dict[str, list[dict]] = {}
    for group, members in sorted(groups.items()):
        part, used, current = 1, 0, []
        for entry in members:
            padded = (entry["size"] + ALIGN - 1) // ALIGN * ALIGN
            if current and used + padded > max_bytes:
                shards[f"{group}-{part}"] = current
                part, used, current = part + 1, 0, []
            current.append(entry)
            used += padded
        shards[group if part == 1 else f"{group}-{part}"] = current
    return shards


def read_manifest(path: Path) -> dict[tuple[int, str], str]:
    names: dict[tuple[int, str], str] = {}
    opener = (lambda: gzip.open(path, "rt", newline="", encoding="utf-8")) if path.suffix == ".gz" \
        else (lambda: path.open(newline="", encoding="utf-8"))
    with opener() as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            relative = row["path"]
            if relative.startswith("_unknown/"):
                continue
            names[(path_hash("/" + relative.lower()), row["sha256"])] = relative
    return names


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def write_idx(index: dict, path: Path) -> None:
    """Compact little-endian index read by the SDK (rex/filesystem/devices/sharded_archive.h).

    magic[8] u32 version u32 align u64 virtual_size u32 shard_count u32 extent_count
    shard_count x { u16 name_len, name[name_len], u64 size }
    extent_count x { u64 virtual_offset, u32 size, u16 shard, u16 reserved, u64 shard_offset }  (sorted)
    """
    shard_ids = {record["name"]: number for number, record in enumerate(index["shards"])}
    # Empty entries (size 0, e.g. .emtm files) own no bytes and may share an offset with a real entry.
    entries = [entry for entry in index["entries"] if entry["size"] > 0]
    out = bytearray(IDX_MAGIC)
    out += struct.pack("<IIQII", INDEX_VERSION, index["align"], index["source"]["bdt_size"],
                       len(index["shards"]), len(entries))
    for record in index["shards"]:
        name = record["file"].encode("utf-8")
        out += struct.pack("<H", len(name)) + name + struct.pack("<Q", record["size"])
    for entry in sorted(entries, key=lambda item: item["orig_offset"]):
        out += struct.pack("<QIHHQ", entry["orig_offset"], entry["size"], shard_ids[entry["shard"]], 0, entry["offset"])
    path.write_bytes(bytes(out))


def read_idx(path: Path) -> dict:
    data = path.read_bytes()
    if data[:8] != IDX_MAGIC:
        raise ValueError("bad shard index magic")
    version, align, virtual_size, shard_count, extent_count = struct.unpack_from("<IIQII", data, 8)
    position, shards, extents = 8 + 24, [], []
    for _ in range(shard_count):
        (length,) = struct.unpack_from("<H", data, position)
        name = data[position + 2:position + 2 + length].decode("utf-8")
        (size,) = struct.unpack_from("<Q", data, position + 2 + length)
        shards.append({"file": name, "size": size})
        position += 2 + length + 8
    for _ in range(extent_count):
        virtual_offset, size, shard, _reserved, shard_offset = struct.unpack_from("<QIHHQ", data, position)
        extents.append({"virtual_offset": virtual_offset, "size": size, "shard": shard, "shard_offset": shard_offset})
        position += 24
    if position != len(data):
        raise ValueError("trailing bytes in shard index")
    return {"version": version, "align": align, "virtual_size": virtual_size, "shards": shards, "extents": extents}


def find_gaps(entries: list[dict], total: int) -> list[tuple[int, int]]:
    """Byte ranges of the archive that no entry owns (e.g. the BDT file header)."""
    gaps, cursor = [], 0
    for entry in sorted((e for e in entries if e["size"] > 0), key=lambda item: item["offset"]):
        if entry["offset"] > cursor:
            gaps.append((cursor, entry["offset"] - cursor))
        cursor = max(cursor, entry["offset"] + entry["size"])
    if cursor < total:
        gaps.append((cursor, total - cursor))
    return gaps


def build(args) -> dict:
    out = args.out.resolve()
    sources = {args.bhd.resolve(), args.bdt.resolve()}
    if out in sources or any(source.parent == out for source in sources):
        raise SystemExit("output directory must not be the source archive directory")
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()) and not args.overwrite:
        raise SystemExit(f"{out} is not empty; use --overwrite to rebuild")
    entries = load_parse()(args.bhd, args.bdt)
    names = read_manifest(args.manifest)
    with args.bdt.open("rb") as bdt:
        for entry in entries:
            bdt.seek(entry["offset"])
            data = bdt.read(entry["size"])
            if len(data) != entry["size"]:
                raise ValueError(f"short read for hash {entry['path_hash']:08x}")
            entry["sha256"] = hashlib.sha256(data).hexdigest()
            entry["path"] = names.get((entry["path_hash"], entry["sha256"]))
        # The guest may read bytes no entry owns (file header); keep them so the virtual file is complete.
        for offset, length in find_gaps(entries, args.bdt.stat().st_size):
            bdt.seek(offset)
            data = bdt.read(length)
            entries.append({"bucket": -1, "entry": -1, "path_hash": 0, "offset": offset, "size": length,
                            "sha256": hashlib.sha256(data).hexdigest(), "path": f"_gap/{offset:x}"})
    shards = plan(entries, args.max_shard_mib * 1024 * 1024)
    shard_records, index_entries = [], []
    with args.bdt.open("rb") as bdt:
        for name, members in shards.items():
            target = out / f"{name}.pak"
            position = 0
            with target.open("wb") as shard:
                for entry in members:
                    pad = -position % ALIGN
                    if pad:
                        shard.write(b"\0" * pad)
                        position += pad
                    bdt.seek(entry["offset"])
                    shard.write(bdt.read(entry["size"]))
                    index_entries.append({
                        "path_hash": entry["path_hash"], "path": entry["path"], "shard": name,
                        "offset": position, "size": entry["size"], "sha256": entry["sha256"],
                        "orig_offset": entry["offset"], "bucket": entry["bucket"], "entry": entry["entry"]})
                    position += entry["size"]
            shard_records.append({"name": name, "file": target.name, "size": position,
                                  "entries": len(members), "sha256": sha256_file(target)})
    index = {"version": INDEX_VERSION, "align": ALIGN,
             "source": {"bhd": args.bhd.name, "bhd_sha256": sha256_file(args.bhd),
                        "bdt": args.bdt.name, "bdt_size": args.bdt.stat().st_size},
             "shards": shard_records, "entries": sorted(index_entries, key=lambda item: item["orig_offset"])}
    (out / "index.json").write_text(json.dumps(index, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    write_idx(index, out / "shards.idx")
    return index


def verify(shard_dir: Path, bdt: Path) -> dict:
    """Re-read every index entry from its shard and from the original archive; both must match the index."""
    index = json.loads((shard_dir / "index.json").read_text(encoding="utf-8"))
    bad, files = [], {}
    shard_files = {record["name"]: record for record in index["shards"]}
    for record in index["shards"]:
        path = shard_dir / record["file"]
        if path.stat().st_size != record["size"] or sha256_file(path) != record["sha256"]:
            bad.append(f"shard {record['name']}: size or sha256 mismatch")
    with bdt.open("rb") as original:
        for entry in index["entries"]:
            if entry["shard"] not in shard_files:
                bad.append(f"{entry['path_hash']:08x}: unknown shard")
                continue
            stream = files.setdefault(entry["shard"], (shard_dir / shard_files[entry["shard"]]["file"]).open("rb"))
            stream.seek(entry["offset"])
            from_shard = stream.read(entry["size"])
            original.seek(entry["orig_offset"])
            from_bdt = original.read(entry["size"])
            digest = hashlib.sha256(from_shard).hexdigest()
            if from_shard != from_bdt or digest != entry["sha256"]:
                bad.append(f"{entry['path_hash']:08x} @ {entry['orig_offset']:#x}: content mismatch")
    for stream in files.values():
        stream.close()
    binary = read_idx(shard_dir / "shards.idx")
    expected = [(e["orig_offset"], e["size"], e["offset"], e["shard"]) for e in sorted(index["entries"], key=lambda i: i["orig_offset"]) if e["size"] > 0]
    names = [record["name"] for record in index["shards"]]
    actual = [(x["virtual_offset"], x["size"], x["shard_offset"], names[x["shard"]]) for x in binary["extents"]]
    if expected != actual or binary["virtual_size"] != index["source"]["bdt_size"]:
        bad.append("shards.idx disagrees with index.json")
    covered = sum(entry["size"] for entry in index["entries"])
    return {"entries": len(index["entries"]), "shards": len(index["shards"]), "bad": bad,
            "covered_bytes": covered, "bdt_size": index["source"]["bdt_size"],
            "covered_fraction": covered / index["source"]["bdt_size"]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--manifest", type=Path, required=True, help="TSV from acv_loose_unpack.py (path, size, sha256)")
    ap.add_argument("--out", type=Path, required=True, help="shard directory (must not be the source directory)")
    ap.add_argument("--max-shard-mib", type=int, default=768)
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--verify", action="store_true", help="after writing, compare every entry with the original bdt")
    args = ap.parse_args()
    index = build(args)
    sizes = sorted(record["size"] for record in index["shards"])
    print(f"shards={len(index['shards'])} entries={len(index['entries'])} "
          f"smallest={sizes[0]} largest={sizes[-1]} unnamed={sum(1 for e in index['entries'] if not e['path'])}")
    if args.verify:
        result = verify(args.out, args.bdt)
        print(json.dumps({key: value for key, value in result.items() if key != "bad"}, indent=1))
        if result["bad"]:
            print("\n".join(result["bad"][:20]), file=sys.stderr)
            return 1
        print("verify: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
