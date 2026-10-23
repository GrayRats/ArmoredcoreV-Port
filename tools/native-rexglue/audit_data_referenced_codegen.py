#!/usr/bin/env python3
"""Audit generated ReXGlue output against acv-data-referenced-functions.toml."""
import json
import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
gen = ROOT / "tmp/acv-rexglue-linux-indexed/generated/conditional-fragments"
config = tomllib.loads((ROOT / "tools/native-rexglue/acv-data-referenced-functions.toml").read_text())
expected = {int(k, 16) for k in config["functions"]}
registered = {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]+),",
                                             (gen / "acv_linux_probe_register.cpp").read_text())}
defined, fatal = set(), []
for path in sorted(gen.glob("acv_linux_probe_recomp.*.cpp")):
    body = path.read_text()
    defined |= {int(x, 16) for x in re.findall(r"^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)", body, re.M)}
    fatal += [f"{path.name}:{body.count(chr(10), 0, m.start()) + 1}: {m.group(0)[:120]}"
              for m in re.finditer(r"REX_FATAL[^\n]*", body)]
manual = set()
for path in [ROOT / "tools/native-rexglue/retail_leaves.cpp", ROOT / "tmp/acv-native-adjusting-thunks.cpp"]:
    manual |= {int(x, 16) for x in re.findall(r"SetFunction\(0x([0-9A-F]{8}),", path.read_text())}
report = {
    "expected": len(expected), "registered_total": len(registered),
    "missing_registration": [hex(a) for a in sorted(expected - registered)],
    "missing_definition": [hex(a) for a in sorted(expected - defined)],
    "manual_now_generated": [hex(a) for a in sorted(manual & registered)],
    "rex_fatal_sites": fatal}
(ROOT / "logs/acv-native-linux/data-referenced-codegen-audit.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({k: (v if not isinstance(v, list) else len(v)) for k, v in report.items()}))
if report["missing_registration"] or report["missing_definition"] or report["manual_now_generated"] or fatal:
    print("AUDIT FAILED", file=sys.stderr)
    raise SystemExit(1)
