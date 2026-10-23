#!/usr/bin/env python3
"""Extract the complete 0x8245FA20 leaf and compare its state to readable C++."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'tmp/native-decomp-8245fa20'
OUTPUT.mkdir(parents=True, exist_ok=True)
GENERATED = ROOT / 'tmp/xenon-out/ppc'
source = GENERATED / 'ppc_recomp.99.cpp'
text = source.read_text()
raw = (ROOT / 'tmp/scan/text.bin').read_bytes()[0x8245fa20 - 0x822b0000:0x8245fae0 - 0x822b0000]
digest = hashlib.sha256(raw).hexdigest()
if len(raw) != 192 or digest != '175a6eea2fdda3048a76c788ec2b8e110f0ba5b8e89476bb5f50c77e7e6da7df':
    raise ValueError('Raw instruction boundary/hash mismatch')
for begin, metadata in struct.iter_unpack('>II', (ROOT / 'tmp/scan/pdata.bin').read_bytes()):
    length = ((metadata >> 8) & 0x3fffff) * 4
    if begin <= 0x8245fa20 < begin + length:
        raise ValueError('Unexpected .pdata owner; review boundary evidence')
match = re.search(r'__attribute__\(\(alias\("__imp__sub_8245FA20"\)\)\) PPC_WEAK_FUNC\(sub_8245FA20\);\n'
                  r'PPC_FUNC_IMPL\(__imp__sub_8245FA20\) \{\n.*?\n\}', text, re.S)
if not match:
    raise ValueError('Generated leaf missing')
block = match.group()
body = block.split('{', 1)[1]
if any(s in body for s in ('ERROR', '.long', 'sub_', 'PPC_CALL')):
    raise ValueError('Selected generated function is incomplete or non-leaf')
comments = re.findall(r'^\t// ', body, re.M)
if len(comments) != 48:
    raise ValueError('Expected 48 instruction comments')
extracted = OUTPUT / 'generated_8245fa20.cpp'
extracted.write_text('#include "host.hpp"\n\n' + block + '\n')
report = {'guest_start': '0x8245FA20', 'guest_end_exclusive': '0x8245FAE0',
          'raw_size': len(raw), 'raw_sha256': digest, 'instruction_count': 48,
          'pdata_owner': None, 'generated_source_line': text[:match.start()].count('\n') + 1,
          'scope': 'state-only generated-vs-handwritten host comparison; no retail/Xenia parity',
          'excluded': ['volatile registers', 'scratch stack', 'CR', 'XER', 'FPSCR', 'NaN payload equivalence'],
          'hashes': {}}
for p in [source, GENERATED / 'ppc_context.h', GENERATED / 'ppc_config.h',
          ROOT / 'tools/native-decomp/acv_8245fa20.hpp', ROOT / 'tools/native-decomp/compare_8245fa20.cpp',
          ROOT / 'tools/native-host-probe/host.hpp', Path(__file__), extracted]:
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
                '-I' + str(ROOT / 'tools/native-host-probe'), '-I' + str(ROOT / 'tools/native-decomp'),
                '-I' + str(GENERATED), '-I' + str(ROOT / 'tools/XenonRecomp/thirdparty/simde'),
                str(extracted), str(ROOT / 'tools/native-decomp/compare_8245fa20.cpp'),
                '-o', str(OUTPUT / 'compare')])
run('execution', [str(OUTPUT / 'compare')])
