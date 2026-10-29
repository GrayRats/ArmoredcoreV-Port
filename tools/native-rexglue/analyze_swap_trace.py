#!/usr/bin/env python3
"""Summarize bounded raw guest swap timestamps; never calls these unique presents."""
import argparse
import json
import math
from pathlib import Path


def summarize(path):
    with path.open() as stream:
        meta = json.loads(next(stream))
        rows = [json.loads(line) for line in stream if line.strip()]
    if meta.get("schema") != 1 or meta.get("kind") != "metadata":
        raise ValueError("Unsupported swap trace schema")
    if len(rows) != meta["samples"] or len(rows) < 2:
        raise ValueError("Trace is truncated or has fewer than two swaps")
    intervals, worst = [], []
    for i, row in enumerate(rows):
        if row.get("kind") != "swap" or not math.isfinite(row["age_s"]) or row["age_s"] < 90:
            raise ValueError("Invalid row or sample before90s")
        a, p, d = (row[key] for key in ("arrival_ns", "paced_ns", "done_ns"))
        if any(not isinstance(v, int) or v < 0 for v in (a, p, d)) or not a <= p <= d:
            raise ValueError("Invalid timestamp order")
        if not i:
            continue
        prev = rows[i-1]
        if prev["done_ns"] > a or row["age_s"] < prev["age_s"]:
            raise ValueError("Non-monotonic trace")
        interval = (p - prev["paced_ns"]) / 1e6
        if interval <= 0:
            raise ValueError("Nonpositive swap interval")
        intervals.append(interval)
        worst.append({"index": i, "age_s": row["age_s"], "interval_ms": interval,
                      "previous_post_pacing_cpu_ms": (prev["done_ns"] - prev["paced_ns"]) / 1e6,
                      "between_swap_calls_ms": (a - prev["done_ns"]) / 1e6,
                      "limiter_cpu_ms": (p - a) / 1e6})
    ordered = sorted(intervals)
    slow_count = math.ceil(len(ordered) * .01)
    mean = sum(ordered) / len(ordered)
    low = 1000 / (sum(ordered[-slow_count:]) / slow_count)
    return {"scope": meta["scope"], "complete": meta["complete"], "stop_reason": meta["stop_reason"],
            "swaps": len(rows), "intervals": len(intervals),
            "swap_rate": 1000 / mean, "mean_ms": mean,
            "p95_ms": ordered[math.ceil(.95 * len(ordered)) - 1],
            "p99_ms": ordered[math.ceil(.99 * len(ordered)) - 1],
            "one_percent_low": low, "slowest_one_percent_count": slow_count,
            "min_ms": ordered[0], "max_ms": ordered[-1],
            "intervals_at_least_ms": {str(t): sum(v >= t for v in intervals) for t in (50, 100, 250, 500)},
            "worst": sorted(worst, key=lambda r: r["interval_ms"], reverse=True)[:20]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = summarize(args.trace)
    encoded = json.dumps(result, indent=2)
    if args.output:
        with args.output.open("x") as stream:
            stream.write(encoded + "\n")
    print(encoded)
    raise SystemExit(0 if result["complete"] else 1)
