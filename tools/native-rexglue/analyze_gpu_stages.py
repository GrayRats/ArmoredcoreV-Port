#!/usr/bin/env python3
"""Summarize gated, inclusive GPU command-thread wall timers.
Observed sessions are not a deterministic frame-for-frame benchmark.
"""
import argparse
import json
import math
from pathlib import Path


def summarize(path):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if not rows:
        raise ValueError(f"{path}: no complete timer windows")
    previous = 90.0
    stages = {}
    duration = 0.0
    for row in rows:
        age, window = row["game_age_s"], row["window_s"]
        if not math.isfinite(age) or age < previous or age < 90:
            raise ValueError(f"{path}: invalid or premature process age {age}")
        if not math.isfinite(window) or window <= 0:
            raise ValueError(f"{path}: invalid window {window}")
        if row["scope"] != "inclusive CPU wall time":
            raise ValueError(f"{path}: unknown timer scope")
        previous = age
        duration += window
        for name, value in row["stages"].items():
            calls, total, maximum = value["calls"], value["total_ms"], value["max_ms"]
            if not isinstance(calls, int) or calls < 0 or not all(math.isfinite(x) and x >= 0 for x in (total, maximum)):
                raise ValueError(f"{path}: invalid metric {name}")
            metric = stages.setdefault(name, {"calls": 0, "total_ms": 0.0, "max_ms": 0.0})
            metric["calls"] += calls
            metric["total_ms"] += total
            metric["max_ms"] = max(metric["max_ms"], maximum)
    for value in stages.values():
        value["wall_percent"] = value["total_ms"] / (duration * 10)
        value["calls_per_second"] = value["calls"] / duration
    return {"source": str(path), "windows": len(rows), "window_seconds": duration,
            "first_record_game_age_s": rows[0]["game_age_s"],
            "last_record_game_age_s": rows[-1]["game_age_s"], "stages": stages}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    report = {"scope": "observed inclusive wall time; nested stages must not be summed; sessions are not identical-input benchmarks",
              "sessions": [summarize(path) for path in args.inputs]}
    args.out.write_text(json.dumps(report, indent=2) + "\n")
    for session in report["sessions"]:
        print(session["source"], "windows", session["windows"], "seconds", round(session["window_seconds"], 3))
        for name, metric in session["stages"].items():
            print(f"  {name}: {metric['wall_percent']:.3f}% wall; {metric['calls']} calls")


if __name__ == "__main__":
    main()
