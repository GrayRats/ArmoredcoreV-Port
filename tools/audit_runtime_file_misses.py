#!/usr/bin/env python3
"""Summarize explicit guest file-open failures in preserved native runtime logs."""
import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
import re

PATTERN = re.compile(r"\[NtCreateFile\] FAILED: path='([^']+)' -> (0x[0-9a-fA-F]+)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    failures = defaultdict(lambda: {"count": 0, "statuses": Counter(), "runs": set()})
    log_count = 0
    log_paths = sorted(args.log_root.glob("*/runtime.log"), key=lambda p: p.stat().st_mtime)
    latest_log = log_paths[-1] if log_paths else None
    latest_failures = Counter()
    for path in log_paths:
        log_count += 1
        run = path.parent.name
        for line in path.read_text(errors="replace").splitlines():
            match = PATTERN.search(line)
            if not match:
                continue
            guest_path, status = match.groups()
            row = failures[guest_path]
            row["count"] += 1
            row["statuses"][status.lower()] += 1
            row["runs"].add(run)
            if path == latest_log:
                latest_failures[(guest_path, status.lower())] += 1
    records = []
    for guest_path, row in sorted(failures.items()):
        records.append({
            "path": guest_path,
            "count": row["count"],
            "statuses": dict(sorted(row["statuses"].items())),
            "run_count": len(row["runs"]),
            "first_run": min(row["runs"]),
            "latest_run": max(row["runs"]),
        })
    report = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "log_root": str(args.log_root),
        "runtime_logs_scanned": log_count,
        "latest_runtime_log": str(latest_log) if latest_log else None,
        "matching_file_open_failures": sum(item["count"] for item in records),
        "unique_guest_paths": len(records),
        "latest_log_file_open_failures": [
            {"path": guest_path, "status": status, "count": count}
            for (guest_path, status), count in sorted(latest_failures.items())
        ],
        "scope": "Explicit NtCreateFile FAILED lines only; VFS root-resolution diagnostics excluded.",
        "failures": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in (
        "runtime_logs_scanned", "latest_runtime_log", "matching_file_open_failures",
        "unique_guest_paths", "latest_log_file_open_failures")}, indent=2))
    for item in records:
        print(f"{item['count']:4d} {item['run_count']:3d} runs {item['path']} {item['statuses']}")


if __name__ == "__main__":
    main()
