#!/usr/bin/env python3
"""Check captured PPC output against a perspective matrix; never modify game data."""
import argparse
import collections
import json
import math
import struct
from pathlib import Path


def analyze(trace):
    if trace["errors"]:
        raise ValueError("Trace has debugger read errors")
    samples = [r for r in trace["samples"] if r["entry"] == "82BC8980"]
    if not samples:
        raise ValueError("No projection helper calls captured")
    cases, worst = {}, 0.0
    for row in samples:
        fov, aspect, near, far = [struct.unpack("<d", bytes.fromhex(row["fpr_le_hex"][f"f{i}"]))[0] for i in range(1, 5)]
        if not all(map(math.isfinite, (fov, aspect, near, far))) or not (0 < fov < math.pi and aspect > 0 and 0 < near < far):
            raise ValueError("Sample outside tested perspective domain")
        actual = struct.unpack(">16f", bytes.fromhex(row["return_block_be_hex"]))
        scale = 1 / math.tan(fov / 2)
        depth = far / (far - near)
        expected = (scale/aspect, 0, 0, 0, 0, scale, 0, 0, 0, 0, depth, 1, 0, 0, -near*depth, 0)
        if not all(map(math.isfinite, actual)):
            raise ValueError("Nonfinite captured matrix")
        error = max(abs(a-b)/max(1, abs(b)) for a, b in zip(actual, expected))
        if error > 2e-6:
            raise ValueError(f"Captured output differs from perspective reference: {error}")
        worst = max(worst, error)
        key = (fov, aspect, near, far)
        cases.setdefault(key, {"vertical_fov_radians": fov, "vertical_fov_degrees": math.degrees(fov),
            "aspect": aspect, "near": near, "far": far, "matrix": actual, "samples": 0})["samples"] += 1
    return {"scope": "Observed recompiled helper output agrees numerically with perspective formula; not original-hardware parity, main-camera attribution, HUD or culling validation",
            "sample_count": len(samples), "max_normalized_error": worst, "tolerance": 2e-6,
            "cases": list(cases.values()),
            "entry_counts": dict(collections.Counter(r["entry"] for r in trace["samples"]))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(json.loads(args.trace.read_text()))
    result["source_trace"] = str(args.trace.resolve())
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
