#!/usr/bin/env python3
"""Check whether the installed ReXGlue PPC test fixture is executable evidence.

Only reads the workspace ReXGlue source and performs host IEEE-754 float
rounding calculations. Does not build or run ReXGlue or the game.
"""
import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEST = ROOT / "tools/rexglue-sdk/tests/ppc/asm/instr_frsqrte.s"
PARSER = ROOT / "tools/rexglue-sdk/src/rexglue/commands/test_recompiler.cpp"
BUILDER = ROOT / "tools/rexglue-sdk/src/codegen/builders/floating_point.cpp"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def main():
    test_text = TEST.read_text()
    parser_text = PARSER.read_text()
    builder_text = BUILDER.read_text()
    pattern = re.compile(
        r"#\s*_\s+REGISTER_IN f1 ([^\s]+).*?"
        r"frsqrte f1, f1.*?"
        r"#\s*_\s+REGISTER_OUT f1 ([^\s]+)", re.S,
    )
    cases = []
    for x, expected in pattern.findall(test_text):
        x_value, expected_value = float(x), float(expected)
        cases.append({
            "input": x_value,
            "fixture_output": expected_value,
            "host_formula_float32": f32(1.0 / math.sqrt(x_value)),
            "fixture_matches_host_formula": expected_value == f32(1.0 / math.sqrt(x_value)),
        })
    parser_guard = "if (line.size() <= 1 || line[1] != '_')" in parser_text
    directives = re.findall(r"^\s*#.*REGISTER_(?:IN|OUT).*?$", test_text, re.M)
    accepted_directives = [line for line in directives if len(line.strip()) > 1 and line.strip()[1] == "_"]
    result = {
        "scope": "static source and host-float audit; fixture provenance and Xenon hardware behavior unverified",
        "reXGlue_git_head": __import__("subprocess").run(
            ["git", "-C", str(ROOT / "tools/rexglue-sdk"), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip(),
        "files": {str(p.relative_to(ROOT)): sha(p) for p in (TEST, PARSER, BUILDER)},
        "parser_guard_requires_second_character_underscore": parser_guard,
        "fixture_directive_lines": directives,
        "accepted_fixture_directives_under_parser_guard": accepted_directives,
        "parsed_test_case_count": len(cases),
        "builder_frsqrte_expression": "double(float(1.0 / sqrt(source.f64)))" if "1.0 / sqrt" in builder_text else None,
        "cases": cases,
        "interpretation": "Fixture comments use '# _', but the test parser requires '#' immediately followed by '_'; therefore these inputs/outputs are not parsed. Fixture origin is unknown. The builder expression is mathematical reciprocal square root rounded to float and differs from all three listed fixture outputs.",
    }
    output = ROOT / "logs/acv-dialect-audit-20261020/rexglue-frsqrte-fixture-audit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"fixture_cases={len(cases)} parsed_directives={len(accepted_directives)} output={output.relative_to(ROOT)}")
    for case in cases:
        print(f"input={case['input']} fixture={case['fixture_output']} host_formula={case['host_formula_float32']} match={case['fixture_matches_host_formula']}")


if __name__ == "__main__":
    main()
