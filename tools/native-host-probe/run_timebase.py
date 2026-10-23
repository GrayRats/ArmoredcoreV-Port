#!/usr/bin/env python3
"""Extract and run the generated 0x82B8F9C0 leaf with a deterministic clock stub."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "tmp/native-host-probe/timebase"
GENERATED_DIR = ROOT / "tmp/xenon-more-mulhd-output"
ADDRESS = 0x82B8F9C0
EXPECTED_WORDS = "7d6c42e6556a003f408200087d6c42e6f9630000386000014e800020"
SOURCE = GENERATED_DIR / "ppc_recomp.490.cpp"
DUMP = ROOT / "tmp/scan/text.bin"
PATTERN = re.compile(
    r'__attribute__\(\(alias\("__imp__sub_82B8F9C0"\)\)\) '
    r'PPC_WEAK_FUNC\(sub_82B8F9C0\);\n'
    r'PPC_FUNC_IMPL\(__imp__sub_82B8F9C0\) \{\n.*?\n\}', re.S,
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


OUTPUT.mkdir(parents=True, exist_ok=True)
text = SOURCE.read_text()
match = PATTERN.search(text)
if not match:
    raise SystemExit("generated function 0x82B8F9C0 not found")
raw = DUMP.read_bytes()
offset = ADDRESS - 0x822B0000
actual_words = raw[offset:offset + 28].hex()
if actual_words != EXPECTED_WORDS:
    raise SystemExit(f"raw PPC mismatch: expected {EXPECTED_WORDS}, got {actual_words}")
block = match.group()
if "__rdtsc()" not in block or "PPC_STORE_U64" not in block or "goto loc_82B8F9D0" not in block:
    raise SystemExit("generated function differs from the expected bounded clock-leaf shape")

generated = OUTPUT / "generated_timebase.cpp"
generated.write_text(
    '#include "host.hpp"\n'
    'uint64_t mock_timebase_read();\n'
    '#define __rdtsc() mock_timebase_read()\n'
    + block + '\n#undef __rdtsc\n'
)
report = {
    "scope": "one generated leaf with a deterministic mock for the source-level __rdtsc hook; not Xenon clock-rate or Xenia parity",
    "guest_va": hex(ADDRESS),
    "raw_words": [actual_words[i:i+8] for i in range(0, len(actual_words), 8)],
    "generated_function_line": text[:match.start()].count("\n") + 1,
    "generated_block_sha256": hashlib.sha256(block.encode()).hexdigest(),
    "inputs": {str(p.relative_to(ROOT)): sha(p) for p in [SOURCE, DUMP, ROOT / "tools/native-host-probe/host.hpp", ROOT / "tools/native-host-probe/timebase_probe.cpp", Path(__file__)]},
}
command = [
    "clang++", "-std=c++20", "-O1", "-g", "-mavx2", "-fno-strict-aliasing",
    "-fsanitize=undefined", "-fno-sanitize-recover=all",
    "-I" + str(ROOT / "tools/native-host-probe"), "-I" + str(GENERATED_DIR),
    str(generated), str(ROOT / "tools/native-host-probe/timebase_probe.cpp"),
    "-o", str(OUTPUT / "timebase_probe"),
]
compiled = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
(OUTPUT / "compile.stdout.txt").write_text(compiled.stdout)
(OUTPUT / "compile.stderr.txt").write_text(compiled.stderr)
report["compile"] = {"command": command, "exit_code": compiled.returncode,
                     "stdout": compiled.stdout, "stderr": compiled.stderr}
if compiled.returncode:
    (OUTPUT / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(compiled.returncode)
executed = subprocess.run([str(OUTPUT / "timebase_probe")], cwd=ROOT, capture_output=True, text=True)
(OUTPUT / "execution.stdout.txt").write_text(executed.stdout)
(OUTPUT / "execution.stderr.txt").write_text(executed.stderr)
report["execution"] = {"exit_code": executed.returncode,
                       "stdout": executed.stdout, "stderr": executed.stderr}
report["artifacts"] = {p.name: sha(p) for p in [generated, OUTPUT / "timebase_probe"]}
(OUTPUT / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print(executed.stdout, end="")
print(executed.stderr, end="")
print("Evidence: " + str((OUTPUT / "verification.json").relative_to(ROOT)))
if executed.returncode:
    raise SystemExit(executed.returncode)
