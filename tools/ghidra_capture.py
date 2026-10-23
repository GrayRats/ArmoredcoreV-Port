#!/usr/bin/env python3
"""Capture bounded ACV evidence from an already running Ghidra RPC session.

Read-only RPC operations; snapshots never replace previous exports. Listing
coverage is checked independently of Ghidra's successful decompilation response.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
XEX_SHA256 = "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b"
PROGRAM = "default.xex"


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def coverage(instructions, start, length):
    """A listing iterator may silently skip undefined bytes or leave the range."""
    end = start + length
    selected = [i for i in instructions if start <= int(i["address"], 16) < end]
    present = {int(i["address"], 16) for i in selected if i["length"] == 4}
    missing = [f"0x{a:08X}" for a in range(start, end, 4) if a not in present]
    return selected, missing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("address", type=lambda s: int(s, 0))
    parser.add_argument("length", type=lambda s: int(s, 0),
                        help="Explicit byte range, 4..4000 bytes; may include padding")
    args = parser.parse_args()
    if args.address % 4 or args.length % 4 or not 4 <= args.length <= 4000:
        parser.error("address and length must be PPC-aligned; length must be 4..4000")
    if not 0 <= args.address < args.address + args.length <= 0x100000000:
        parser.error("range must fit the 32-bit guest address space")

    xex = ROOT / "games/acv/extracted/default.xex"
    with xex.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != XEX_SHA256:
        raise RuntimeError("Staged ACV executable hash differs from the recorded input")

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    destination = ROOT / "decompilation/evidence/acv" / f"{args.address:08X}-{stamp}"
    destination.mkdir(parents=True, exist_ok=False)
    report = {"schema": 1, "created_utc": stamp, "program": PROGRAM,
              "staged_xex": str(xex.relative_to(ROOT)), "staged_sha256": digest,
              "identity_limit": "Staged file hash does not independently verify the open Ghidra program",
              "start": f"0x{args.address:08X}", "length": args.length,
              "warnings": [], "capture_complete": False}

    def rpc(label, *argv, required=True):
        result = subprocess.run([str(ROOT / "scripts/ghidra-rpc.sh"), *argv],
                                cwd=ROOT, capture_output=True, text=True, timeout=45,
                                env={**os.environ, "GHIDRA_RPC_NO_AUTOSTART": "1"})
        (destination / f"{label}.json").write_text(result.stdout, encoding="utf-8")
        if result.stderr:
            (destination / f"{label}.stderr.log").write_text(result.stderr, encoding="utf-8")
        data = json.loads(result.stdout)
        if result.returncode or not data.get("ok"):
            report["warnings"].append(f"{label}: RPC failed (see saved response)")
            if required:
                raise RuntimeError(f"{label}: RPC failed")
            return {}
        return data["result"]

    try:
        status = rpc("status", "status")
        expected_project = ROOT / "tmp/ghidra/isolated-projects/ACV_RETRY.gpr"
        if not status.get("running"):
            raise RuntimeError("RPC is stopped; capture does not auto-start or restart Ghidra")
        if Path(status["project"]).resolve() != expected_project.resolve():
            raise RuntimeError("RPC is attached to a different project")
        programs = rpc("project", "list-project-programs")
        if programs.get("warning") or not any(
                p["path"] == "/default.xex" and p.get("loaded")
                for p in programs["programs"]):
            raise RuntimeError("Expected /default.xex is not loaded in the expected project")
        report["metadata"] = rpc("metadata", "metadata", PROGRAM)
        segments = rpc("memory-map", "memory-map", PROGRAM)["segments"]
        if not any(s["initialized"] and s["execute"]
                   and int(s["start"], 16) <= args.address
                   and args.address + args.length - 1 <= int(s["end"], 16)
                   for s in segments):
            raise RuntimeError("Range must fit one initialized executable memory block")

        address = report["start"]
        raw = rpc("bytes", "read-bytes", PROGRAM, address, str(args.length))
        payload = bytes.fromhex(raw["hex"])
        if len(payload) != args.length or int(raw["address"], 16) != args.address:
            raise RuntimeError("RPC returned unexpected memory range")
        report["range_sha256"] = hashlib.sha256(payload).hexdigest()
        (destination / "bytes.bin").write_bytes(payload)
        listing = rpc("disassembly", "disassemble", PROGRAM, address,
                      "--count", str(args.length // 4), "--with-instructions")
        selected, missing = coverage(listing["instructions"], args.address, args.length)
        report["undefined_instruction_addresses"] = missing
        report["listing_covers_requested_range"] = not missing
        for instruction in selected:
            offset = int(instruction["address"], 16) - args.address
            if bytes.fromhex(instruction["bytes"]) != payload[offset:offset + instruction["length"]]:
                raise RuntimeError("Listing bytes disagree with memory snapshot")
        (destination / "listing.txt").write_text("\n".join(
            f"{i['address']}  {i['bytes']}  {i['mnemonic']} {i['operands']}"
            for i in selected) + "\n", encoding="utf-8")
        if missing:
            report["warnings"].append("Listing has gaps; padding and missing instructions require review")
        functions = rpc("functions", "functions", PROGRAM, "--address-min", address,
                        "--address-max", address, "--with-body", "--limit", "1")["functions"]
        report["entry_function"] = functions[0] if functions else None
        if not functions:
            report["warnings"].append("No function defined at the requested entry")
        else:
            if functions[0]["body_size"] < args.length:
                report["warnings"].append("Function body is smaller than requested range; inspect truncation/padding")
            decompiled = rpc("decompile", "decompile", PROGRAM, address, "--timeout", "30", required=False)
            if "c_code" in decompiled:
                if "WARNING" in decompiled["c_code"]:
                    report["warnings"].append("Decompiler emitted warnings")
                (destination / "decompiled.c").write_text(
                    "/* Ghidra pseudocode only; review report.json and listing.txt before interpreting. */\n"
                    + decompiled["c_code"], encoding="utf-8")
        rpc("xrefs-to", "xrefs-to", PROGRAM, address, "--limit", "100")
        report["capture_complete"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        write_json(destination / "report.json", report)
        print(json.dumps({"destination": str(destination), **report}, indent=2))
    return 0 if report["capture_complete"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
