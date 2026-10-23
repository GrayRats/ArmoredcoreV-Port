#!/usr/bin/env python3
"""Rank non-stack VMX128 offset-pattern candidates by owner and fields.

This is a read-only summary of tools/scan output. It does not prove pointer
provenance or identify a semantic consumer.
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "candidates",
        nargs="?",
        type=Path,
        default=Path("logs/acv-math-followup-20261021/vmx128-field-read-candidates.json"),
    )
    parser.add_argument("--limit", type=int, default=40)
    args = parser.parse_args()

    source = json.loads(args.candidates.read_text())
    owners: dict[str, dict[str, object]] = defaultdict(
        lambda: {"count": 0, "fields": set(), "sites": []}
    )
    nonstack = 0
    for candidate in source["candidates"]:
        if candidate["source_base_register"] == 1:
            continue
        nonstack += 1
        owner = candidate.get("owner") or {}
        key = owner.get("begin", "no-pdata-owner")
        bucket = owners[key]
        bucket["count"] += 1
        bucket["fields"].add(candidate["field_displacement"])
        bucket["sites"].append(candidate["addi_address"])

    ranked = sorted(
        owners.items(),
        key=lambda item: (len(item[1]["fields"]), item[1]["count"]),
        reverse=True,
    )[: args.limit]
    print(
        json.dumps(
            {
                "source_candidate_count": source["candidate_count"],
                "nonstack_candidate_count": nonstack,
                "nonstack_owner_count": len(owners),
                "ranked_owners": [
                    {
                        "owner": owner,
                        "candidate_count": info["count"],
                        "field_displacements": sorted(info["fields"]),
                        "sites": info["sites"][:12],
                    }
                    for owner, info in ranked
                ],
                "interpretation": "offset pattern only; aliases require separate dataflow proof",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
