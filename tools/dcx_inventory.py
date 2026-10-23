#!/usr/bin/env python3
"""Decode and inventory DCX/DCP DFLT BDT entries without writing their payloads."""

from __future__ import annotations

import argparse
import csv
import hashlib
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bhd5_index import decode_dcx_dflt, parse as parse_bhd5
from bnd3_x360 import parse as parse_bnd3
from tpf_x360 import parse as parse_tpf


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bhd", type=Path)
    ap.add_argument("bdt", type=Path)
    ap.add_argument("--manifest", required=True, type=Path)
    args = ap.parse_args()
    try:
        workspace = Path(__file__).resolve().parents[1]
        manifest = args.manifest.resolve()
        if not manifest.is_relative_to(workspace):
            raise ValueError("manifest must stay inside workspace")
        if args.manifest.is_symlink():
            raise ValueError("manifest cannot be a symlink")
        manifest.parent.mkdir(parents=True, exist_ok=True)
        entries = [e for e in parse_bhd5(args.bhd, args.bdt) if str(e["signature"]).startswith("44435800")]
        magics: Counter[str] = Counter()
        parsed_kinds: Counter[str] = Counter()
        child_total = 0
        expanded_total = 0
        with args.bdt.open("rb") as source, manifest.open("w", newline="", encoding="utf-8") as out:
            writer = csv.DictWriter(out, fieldnames=["path_hash", "stored_size", "decoded_size", "decoded_signature", "parsed_kind", "child_count", "decoded_sha256"], delimiter="\t")
            writer.writeheader()
            for entry in entries:
                source.seek(int(entry["offset"]))
                compressed = source.read(int(entry["size"]))
                if len(compressed) != int(entry["size"]):
                    raise ValueError(f'short read for hash {entry["path_hash"]:08x}')
                decoded = decode_dcx_dflt(compressed)
                signature = decoded[:8].hex()
                magics[decoded[:4].hex()] += 1
                parsed_kind = "signature-only"
                child_count = 0
                if decoded[:4] == b"TPF\0":
                    try:
                        _, children = parse_tpf(decoded)
                        parsed_kind = f"TPF-X360-platform-{decoded[12]}"
                        child_count = len(children)
                    except ValueError as ex:
                        parsed_kind = f"TPF-unparsed:{ex}"
                elif decoded[:4] == b"BND3":
                    try:
                        _, children = parse_bnd3(decoded)
                        parsed_kind = f"BND3-format-{decoded[12]:02x}"
                        child_count = len(children)
                    except ValueError as ex:
                        parsed_kind = f"BND3-unparsed-format-{decoded[12]:02x}:{ex}"
                parsed_kinds[parsed_kind] += 1
                child_total += child_count
                expanded_total += len(decoded)
                writer.writerow({
                    "path_hash": f'{int(entry["path_hash"]):08x}',
                    "stored_size": len(compressed), "decoded_size": len(decoded),
                    "decoded_signature": signature, "parsed_kind": parsed_kind,
                    "child_count": child_count,
                    "decoded_sha256": hashlib.sha256(decoded).hexdigest(),
                })
        print(f"validated DCX/DCP DFLT entries: {len(entries)}")
        print(f"decoded bytes inventoried: {expanded_total}")
        print(f"decoded signatures: {magics.most_common(20)}")
        print(f"format parser outcomes: {parsed_kinds.most_common(20)}")
        print(f"children in parsed TPF/BND3 containers: {child_total}")
        print(f"manifest: {manifest}")
        return 0
    except (OSError, ValueError) as ex:
        print(f"dcx_inventory: {ex}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
