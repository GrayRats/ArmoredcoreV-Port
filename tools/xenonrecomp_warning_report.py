#!/usr/bin/env python3
"""Correlate XenonRecomp warnings with saved PPC text and .pdata dumps."""
import argparse
import bisect
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import struct

TEXT_BASE = 0x822B0000


def function_bounds(path):
    data = path.read_bytes()
    entries = []
    for offset in range(0, len(data) - 7, 8):
        begin, metadata = struct.unpack_from(">II", data, offset)
        length = ((metadata >> 8) & 0x3FFFFF) * 4
        if length:
            entries.append((begin, length))
    return sorted(entries)


def category(name):
    lower = name.lower()
    if "128" in lower or lower.startswith(("v", "lv", "stve")):
        return "vector/vmx"
    if lower.startswith(("bdz", "bdnz", "bgt", "blt", "beq", "bne", "bso", "bns")):
        return "conditional branch"
    if lower.endswith("u") or lower.endswith("ux") or lower.endswith("u."):
        return "update-form load/store"
    if lower in {"eqv", "cror", "crorc", "subfze", "addme", "addc", "mulhd"}:
        return "integer/condition arithmetic"
    return "other"


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("text", type=Path)
parser.add_argument("pdata", type=Path)
parser.add_argument("recompiler_log", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()

functions = function_bounds(args.pdata)
starts = [begin for begin, _ in functions]
text = args.text.read_bytes()
records = []
for line_number, line in enumerate(args.recompiler_log.open(errors="replace"), 1):
    match = re.search(r"Unrecognized instruction at 0x([0-9A-Fa-f]+): (\S+)", line)
    if not match:
        continue
    address, mnemonic = int(match.group(1), 16), match.group(2)
    owner_index = bisect.bisect_right(starts, address) - 1
    owner = None
    if owner_index >= 0:
        begin, length = functions[owner_index]
        if address < begin + length:
            owner = {"begin": begin, "length": length}
    offset = address - TEXT_BASE
    word = int.from_bytes(text[offset:offset + 4], "big") if 0 <= offset <= len(text) - 4 else None
    records.append({"address": address, "mnemonic": mnemonic, "word": word,
                    "pdata_owner": owner, "category": category(mnemonic),
                    "log_line": line_number})

by_address = {}
for record in records:
    by_address.setdefault(record["address"], record)
categories = Counter(record["category"] for record in by_address.values())
mnemonics = Counter(record["mnemonic"] for record in by_address.values())
functions_by_owner = defaultdict(list)
for record in by_address.values():
    if record["pdata_owner"]:
        functions_by_owner[record["pdata_owner"]["begin"]].append(record)

result = {
    "inputs": {"text": str(args.text), "pdata": str(args.pdata),
               "recompiler_log": str(args.recompiler_log)},
    "warning_messages": len(records),
    "unique_addresses": len(by_address),
    "missing_words_from_text_dump": sum(record["word"] is None for record in by_address.values()),
    "unique_addresses_without_pdata_owner": sum(record["pdata_owner"] is None for record in by_address.values()),
    "categories": categories.most_common(),
    "mnemonics": mnemonics.most_common(),
    "functions_with_most_warnings": [
        {"begin": begin, "length": records[0]["pdata_owner"]["length"],
         "unique_warning_addresses": len(records),
         "mnemonics": Counter(record["mnemonic"] for record in records).most_common(),
         "examples": [{"address": record["address"], "word": record["word"],
                       "mnemonic": record["mnemonic"]} for record in records[:12]]}
        for begin, records in sorted(functions_by_owner.items(), key=lambda item: -len(item[1]))[:100]
    ],
    "warning_addresses": list(by_address.values()),
}
args.output.write_text(json.dumps(result, indent=2))
print(json.dumps({key: result[key] for key in (
    "warning_messages", "unique_addresses", "missing_words_from_text_dump",
    "unique_addresses_without_pdata_owner", "categories", "mnemonics")}, indent=2))
