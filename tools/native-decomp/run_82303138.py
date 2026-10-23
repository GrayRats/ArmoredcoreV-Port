#!/usr/bin/env python3
"""Validate a bounded wrapper against generated C++, with an observed-call stub."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'tmp/native-decomp-82303138'
OUTPUT.mkdir(parents=True, exist_ok=True)
GENERATED = ROOT / 'tmp/xenon-out/ppc'
source = GENERATED / 'ppc_recomp.17.cpp'
raw = (ROOT / 'tmp/scan/text.bin').read_bytes()[0x53138:0x5318c]
digest = hashlib.sha256(raw).hexdigest()
if len(raw) != 84 or digest != '65e48f3381b7e7f2bc6f44c8ffc106d3ebd080ff5f9e18ac4dced9e0628bcd19':
    raise ValueError('Raw boundary/hash mismatch')
owners = []
for begin, metadata in struct.iter_unpack('>II', (ROOT / 'tmp/scan/pdata.bin').read_bytes()):
    end = begin + ((metadata >> 8) & 0x3fffff) * 4
    if begin <= 0x82303138 < end:
        owners.append((begin, end))
if owners != [(0x82303138, 0x8230318c)]:
    raise ValueError('Unexpected .pdata boundary')
text = source.read_text()
match = re.search(r'__attribute__\(\(alias\("__imp__sub_82303138"\)\)\) PPC_WEAK_FUNC\(sub_82303138\);\n'
                  r'PPC_FUNC_IMPL\(__imp__sub_82303138\) \{\n.*?\n\}', text, re.S)
if not match:
    raise ValueError('Missing generated wrapper')
block = match.group()
body = block.split('{', 1)[1]
if any(marker in body for marker in ('ERROR', '.long', 'PPC_CALL')):
    raise ValueError('Incomplete/indirect generated wrapper')
if re.findall(r'\bsub_([0-9A-F]+)\(ctx, base\)', body) != ['82302520']:
    raise ValueError('Unexpected dependency set')
if len(re.findall(r'^\t// ', body, re.M)) != 21:
    raise ValueError('Instruction count mismatch')
# The independent full raw listing is retained separately from generated C++.
disasm = subprocess.run(['python3', 'tools/ppc_dis.py', 'tmp/scan/text.bin',
                         '0x82303138', '0x8230318c'], cwd=ROOT, text=True,
                        capture_output=True, check=True).stdout
if '.long' in disasm or len(disasm.splitlines()) != 21:
    raise ValueError('Incomplete original instruction listing')
(OUTPUT / 'retail.asm').write_text(disasm)
extracted = OUTPUT / 'generated_82303138.cpp'
extracted.write_text('#include "host.hpp"\nPPC_EXTERN_FUNC(sub_82302520);\n\n' + block + '\n')
report = {
    'guest_start': '0x82303138', 'guest_end_exclusive': '0x8230318C',
    'raw_size': len(raw), 'raw_sha256': digest, 'instruction_count': 21,
    'pdata_boundary_confirmed': True,
    'generated_source_line': text[:match.start()].count('\n') + 1,
    'scope': 'object bytes and selected callee inputs; generated-vs-readable Linux host comparison',
    'external_dependency': '0x82302520 instrumented stub, original implementation NOT executed',
    'excluded': ['return/volatile registers', 'scratch stack', 'CR/XER/FPSCR',
                 'other callee-visible registers/stack', 'original callee behavior',
                 'Xenia/hardware parity', 'Windows execution'],
    'hashes': {},
}
for p in [source, GENERATED / 'ppc_context.h', GENERATED / 'ppc_config.h',
          ROOT / 'tools/native-decomp/acv_82303138.hpp',
          ROOT / 'tools/native-decomp/compare_82303138.cpp', Path(__file__),
          ROOT / 'tools/native-host-probe/host.hpp', extracted, OUTPUT / 'retail.asm']:
    report['hashes'][str(p.relative_to(ROOT))] = hashlib.sha256(p.read_bytes()).hexdigest()


def run(stage, command):
    process = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    (OUTPUT / f'{stage}.stdout.txt').write_text(process.stdout)
    (OUTPUT / f'{stage}.stderr.txt').write_text(process.stderr)
    report[stage] = {'command': list(map(str, command)), 'exit_code': process.returncode,
                     'stdout': process.stdout, 'stderr': process.stderr}
    (OUTPUT / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(process.stdout, end='')
    print(process.stderr, end='')
    process.check_returncode()


run('compiler_version', ['clang++', '--version'])
run('compile', ['clang++', '-std=c++20', '-O2', '-g', '-mavx2', '-ffp-contract=off',
                '-fno-strict-aliasing', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-I' + str(ROOT / 'tools/native-host-probe'),
                '-I' + str(ROOT / 'tools/native-decomp'), '-I' + str(GENERATED),
                '-I' + str(ROOT / 'tools/XenonRecomp/thirdparty/simde'),
                str(extracted), str(ROOT / 'tools/native-decomp/compare_82303138.cpp'),
                '-o', str(OUTPUT / 'compare')])
run('execution', [str(OUTPUT / 'compare')])
summary = json.loads(report['execution']['stdout'])
if summary != {'cases': 10080, 'delegated': 5016, 'writes_only': 5064, 'mismatches': 0}:
    raise ValueError('Unexpected completed test coverage')
