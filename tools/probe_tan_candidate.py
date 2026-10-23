#!/usr/bin/env python3
"""Host-only plausibility probe for the instruction-derived 0x830D6AB0 model.

Reads the saved big-endian constant bytes. This does not execute PPC or claim
retail/Xenia parity; Python round() is used as an explicit RN-even hypothesis
for fctid because the guest FPSCR mode is not known.
"""

from __future__ import annotations

import argparse
import json
import math
import random
import struct
from pathlib import Path


BASE = 0x8216C598
FLOAT_OFFSETS = {
    "two_over_pi": 0x00,
    "threshold": 0x10,
    "c6": 0x40,
    "c7": 0x60,
    "c8": 0x68,
    "c9": 0x50,
    "c10": 0x20,
    "c11": 0x70,
    "c0": 0x30,
    "c5": 0x58,
}


def read_snapshot(path: Path) -> tuple[int, bytes]:
    evidence = json.loads(path.read_text())
    result = evidence.get("result", {})
    return int(result["address"], 16), bytes.fromhex(result["hex"])


def load_constants(path: Path, pi2_path: Path) -> dict[str, float]:
    base, raw = read_snapshot(path)
    if base != BASE or len(raw) < 0x80:
        raise ValueError(f"expected at least 0x80 bytes at 0x{BASE:08X}")
    pi2_base, pi2_raw = read_snapshot(pi2_path)
    values = {
        name: struct.unpack_from(">d", raw, offset)[0]
        for name, offset in FLOAT_OFFSETS.items()
    }
    for name, address in (("pio2_hi", 0x835A6C38), ("pio2_lo", 0x835A6C40)):
        offset = address - pi2_base
        if offset < 0 or offset + 8 > len(pi2_raw):
            raise ValueError(f"constant {name} lies outside {pi2_path}")
        values[name] = struct.unpack_from(">d", pi2_raw, offset)[0]
    return values


def candidate_tan(x: float, c: dict[str, float]) -> float:
    # Mirrors the visible fused PPC multiply-add grouping. The integer
    # conversion uses host RN-even as a documented, unverified assumption.
    n = round(x * c["two_over_pi"])
    reduced = math.fma(-c["pio2_hi"], n, x)
    reduced = math.fma(-c["pio2_lo"], n, reduced)
    z = reduced * reduced

    p = math.fma(-z, c["c11"], c["c10"])
    q = math.fma(z, c["c9"], -c["c8"])
    p = math.fma(p, z, -c["c7"])
    q = math.fma(q, z, c["c6"])
    p = math.fma(p, z, c["c0"])
    q = math.fma(q, z, -c["c5"])
    numerator = p * reduced
    denominator = math.fma(q, z, c["c0"])
    value = numerator / denominator if n % 2 == 0 else -denominator / numerator
    return math.nan if abs(x) >= c["threshold"] else value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--constants",
        type=Path,
        default=Path("logs/acv-math-followup-20261021/constants-8216c598.txt"),
    )
    parser.add_argument(
        "--pi2-constants",
        type=Path,
        default=Path("logs/acv-math-followup-20261021/constants-835a6c20.txt"),
    )
    parser.add_argument("--samples", type=int, default=200_000)
    parser.add_argument("--limit", type=float, default=1_000.0)
    args = parser.parse_args()
    c = load_constants(args.constants, args.pi2_constants)

    rng = random.Random(0x830D6AB0)
    max_normalized_error = 0.0
    max_absolute_error = 0.0
    max_error_input = 0.0
    checked = 0
    for _ in range(args.samples):
        x = rng.uniform(-args.limit, args.limit)
        actual = candidate_tan(x, c)
        expected = math.tan(x)
        absolute_error = abs(actual - expected)
        normalized_error = absolute_error / (1.0 + abs(expected))
        if normalized_error > max_normalized_error:
            max_normalized_error = normalized_error
            max_absolute_error = absolute_error
            max_error_input = x
        checked += 1

    print(
        json.dumps(
            {
                "candidate": "0x830D6AB0",
                "interpretation": "tan-like hypothesis only",
                "samples": checked,
                "uniform_input_range": [-args.limit, args.limit],
                "max_normalized_error": max_normalized_error,
                "absolute_error_at_max": max_absolute_error,
                "input_at_max": max_error_input,
                "rounding_assumption": "host round-to-nearest-even; guest FPSCR unknown",
                "parity_claim": False,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
