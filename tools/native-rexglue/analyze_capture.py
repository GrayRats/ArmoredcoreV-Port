#!/usr/bin/env python3
"""Read one RDC into numeric evidence; no screenshots or image exports.

Invoke through tools/decomp-logging/cli.mjs. XML goes to tmp/, reports beside
the capture. Replay is opt-in because it uses the GPU and should follow game exit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run_json(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    if result.returncode:
        print(result.stdout, file=sys.stderr, end="")
        result.check_returncode()
    return json.loads(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--replay", action="store_true")
    parser.add_argument("--event", type=int, help="replay one action, default final draw")
    parser.add_argument("--mesh", action="store_true", help="sample post-VS float4 slots; needs --event")
    args = parser.parse_args()
    source = args.capture.resolve()
    if not any(source.is_relative_to(ROOT / d) for d in ("logs", "tmp")):
        parser.error("capture must be inside workspace logs/ or tmp/")
    if not source.is_file() or source.suffix != ".rdc":
        parser.error("expected an existing .rdc capture")
    if args.event is not None and (not args.replay or not 0 <= args.event <= 0xFFFFFFFF):
        parser.error("--event requires --replay and a uint32 action ID")
    if args.mesh and args.event is None:
        parser.error("--mesh requires --event and --replay")
    fingerprint = digest(source)
    xml = ROOT / "tmp" / f"acv-capture-{fingerprint[:24]}.xml"
    subprocess.run(["renderdoccmd", "convert", "-f", str(source), "-o", str(xml),
                    "-c", "xml"], check=True, timeout=90, stdout=sys.stderr)
    summary = run_json([sys.executable, str(Path(__file__).with_name("summarize_capture.py")), str(xml)])
    report = {"capture": str(source.relative_to(ROOT)), "capture_sha256": fingerprint,
              "scope": "recorded commands and optional replay data; gameplay/parity unverified",
              "summary": summary}
    if args.replay:
        inspector = ROOT / "tmp/acv_renderdoc_replay"
        if not inspector.is_file():
            parser.error("build replay_inspect.cpp first; see docs/NATIVE_REXGLUE_LINUX.md")
        command = [str(inspector), str(source)]
        if args.event is not None:
            command.append(str(args.event))
        if args.mesh:
            command.append("--mesh")
        report["inspector_sha256"] = digest(inspector)
        report["replay"] = run_json(command)
    if digest(source) != fingerprint:
        raise RuntimeError("capture changed during analysis; report not accepted")
    suffix = ".analysis.json" if args.event is None else f".event{args.event}.analysis.json"
    output = source.with_suffix(suffix)
    output.write_text(json.dumps(report, indent=2) + "\n")
    commands = summary["commands"]
    print(json.dumps({"report": str(output.relative_to(ROOT)),
                      "draw": commands.get("vkCmdDraw", 0),
                      "indexed": commands.get("vkCmdDrawIndexed", 0),
                      "dispatch": commands.get("vkCmdDispatch", 0),
                      "replayed": args.replay}))


if __name__ == "__main__":
    main()
