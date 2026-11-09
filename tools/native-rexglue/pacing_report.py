#!/usr/bin/env python3
"""Summarise '[acv-display] pacing' lines of runtime logs, to compare runs (for example DX12 before/after a change).

Usage: pacing_report.py LOG [LOG ...]    (paths or globs; newest last)
Each pacing line covers 300 guest swaps. A run is judged by the windows after the loading phase, so
--skip-seconds drops the first N seconds of the log (default 120: loading and the first scenes).
The guest swap cadence is what the project measures (docs/project/AGENTS.md), not host FPS.
"""
import argparse, glob, re, statistics, sys
from datetime import datetime
from pathlib import Path

LINE = re.compile(r"^\[(?P<time>[0-9-]+ [0-9:.]+)\].*\[acv-display\] pacing samples=\d+ limit=\d+ "
                  r"fps=(?P<fps>[0-9.]+) mean_ms=(?P<mean>[0-9.]+) p95_ms=(?P<p95>[0-9.]+) "
                  r"p99_ms=(?P<p99>[0-9.]+) min_ms=(?P<min>[0-9.]+) max_ms=(?P<max>[0-9.]+)")
START = re.compile(r"^\[(?P<time>[0-9-]+ [0-9:.]+)\]")


def parse_time(text):
    return datetime.strptime(text, "%Y-%m-%d %H:%M:%S.%f")


def windows(path):
    start, rows = None, []
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        if start is None:
            first = START.match(line)
            if first:
                start = parse_time(first["time"])
        m = LINE.match(line)
        if m and start is not None:
            rows.append({"t": (parse_time(m["time"]) - start).total_seconds(),
                         **{k: float(m[k]) for k in ("fps", "mean", "p95", "p99", "min", "max")}})
    return rows


def summarize(path, skip):
    rows = [r for r in windows(path) if r["t"] >= skip]
    if not rows:
        return None
    return {"windows": len(rows), "seconds": round(rows[-1]["t"] - rows[0]["t"]),
            "fps_mean": statistics.fmean(r["fps"] for r in rows),
            "fps_min": min(r["fps"] for r in rows),
            "p99_median": statistics.median(r["p99"] for r in rows),
            "p99_worst": max(r["p99"] for r in rows),
            "max_worst": max(r["max"] for r in rows),
            "stalls_100ms": sum(r["max"] > 100 for r in rows),
            "stalls_500ms": sum(r["max"] > 500 for r in rows)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--skip-seconds", type=float, default=120)
    args = ap.parse_args()
    paths = [p for pattern in args.logs for p in (sorted(glob.glob(pattern)) or [pattern])]
    print(f"{'log':<46} {'win':>3} {'sec':>4} {'fps avg':>7} {'fps min':>7} {'p99 med':>7} {'p99 max':>7} "
          f"{'max ms':>7} {'>100ms':>6} {'>500ms':>6}")
    for path in paths:
        s = summarize(path, args.skip_seconds)
        name = Path(path).name[:46]
        if s is None:
            print(f"{name:<46} no pacing windows after {args.skip_seconds:.0f} s")
            continue
        print(f"{name:<46} {s['windows']:>3} {s['seconds']:>4} {s['fps_mean']:>7.1f} {s['fps_min']:>7.1f} "
              f"{s['p99_median']:>7.1f} {s['p99_worst']:>7.1f} {s['max_worst']:>7.0f} "
              f"{s['stalls_100ms']:>6} {s['stalls_500ms']:>6}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
