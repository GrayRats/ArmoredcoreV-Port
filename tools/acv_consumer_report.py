#!/usr/bin/env python3
"""Summarize saved ACV trace/memory evidence; never attach to a process."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("trace", type=Path)
parser.add_argument("samples", type=Path)
args = parser.parse_args()
counts = Counter()
pending = {}
pairs = 0
for line in args.trace.open(errors="replace"):
    match = re.search(
        r"f:(\d+) ([0-9A-F]{8}) log_lr_at_instruction: "
        r"lr=([0-9A-F]+) r3=([0-9A-F]+) r4=([0-9A-F]+)", line)
    if not match:
        continue
    frame, thread, lr, r3, r4 = match.groups()
    counts[thread, lr] += 1
    if lr == "82460850":
        pending[thread] = (frame, r3)
    elif lr == "824608D4":
        pairs += pending.pop(thread, None) == (frame, r4)

rows = json.loads(args.samples.read_text())
objects = defaultdict(list)
for row in rows:
    for sample in row["samples"]:
        if "error" not in sample:
            objects[sample["S"]].append((row, sample))
results = []
for address, samples in objects.items():
    changes = []
    previous = None
    for row, sample in samples:
        if previous is None or sample["raw"] != previous["raw"]:
            changes.append({
                "monotonic_ns": row["monotonic_ns"],
                "log_offset": row["log_offset"],
                "last_observed_frame": sample.get("frame"),
                "active": sample["active"], "mode": sample["mode"],
                "counter": sample["counter"], "period": sample["period"],
                "elapsed": sample["elapsed"], "raw": sample["raw"],
                "steps": row["steps"],
            })
            previous = sample
    results.append({"S": address, "sample_count": len(samples), "changes": changes})
print(json.dumps({
    "trace": str(args.trace), "memory_samples": str(args.samples),
    "limitation": "Reads are asynchronous, not atomic or breakpoint-synchronized; "
                  "last observed frame is a log association, not a memory snapshot frame. "
                  "Object lifetime and address reuse are not tracked.",
    "trace_counts": [{"thread": t, "lr": lr, "hits": n}
                     for (t, lr), n in sorted(counts.items())],
    "matched_getter_update_pairs": pairs,
    "objects": results,
}, indent=2))
