#!/usr/bin/env python3
"""Bounded scalar reconstruction and explicit arithmetic-rounding experiment."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'tmp/native-decomp-830d68f0'
OUTPUT.mkdir(parents=True, exist_ok=True)
GENERATED = ROOT / 'tmp/xenon-out/ppc'
source = GENERATED / 'ppc_recomp.658.cpp'
evidence = ROOT / 'logs/acv-callee-20261021'
raw = (ROOT / 'tmp/scan/text.bin').read_bytes()[0x830d68f0-0x822b0000:0x830d69d0-0x822b0000]
digest = hashlib.sha256(raw).hexdigest()
if len(raw) != 224 or digest != '80cfee645671f77d696bcdb90e6418dcfd5406eb56d6d427aac82c9b123f6eac':
    raise ValueError('Raw boundary/hash mismatch')
owners = []
for begin, metadata in struct.iter_unpack('>II', (ROOT / 'tmp/scan/pdata.bin').read_bytes()):
    end = begin + ((metadata >> 8) & 0x3fffff) * 4
    if begin <= 0x830d68f0 < end:
        owners.append((begin, end))
if owners != [(0x830d68f0, 0x830d69d0)]:
    raise ValueError('Unexpected .pdata boundary')
text = source.read_text()
match = re.search(r'__attribute__\(\(alias\("__imp__sub_830D68F0"\)\)\) PPC_WEAK_FUNC\(sub_830D68F0\);\n'
                  r'PPC_FUNC_IMPL\(__imp__sub_830D68F0\) \{\n.*?\n\}', text, re.S)
if not match:
    raise ValueError('Generated scalar missing')
block = match.group()
body = block.split('{', 1)[1]
if any(x in body for x in ('ERROR', '.long', 'PPC_CALL', 'sub_')):
    raise ValueError('Incomplete/non-leaf generated function')
if len(re.findall(r'^\t// ', body, re.M)) != 56:
    raise ValueError('Instruction count mismatch')
records = []
paths = []
for address, name, size in [(0x835a6bc0, 'constants-835a6bc0.json', 120),
                            (0x835a7490, 'constant-835a7490.json', 8),
                            (0x820018e0, 'constant-820018e0.json', 8)]:
    path = evidence / name
    obj = json.loads(path.read_text())
    if not obj['ok'] or int(obj['result']['address'], 16) != address:
        raise ValueError('Wrong constant read response')
    data = bytes.fromhex(obj['result']['hex'])
    if len(data) != size:
        raise ValueError('Truncated constant bytes')
    records.append((address, data))
    paths.append(path)
table = records[0][1]
if table.hex() != ('3ff921fb54442d183fd45f306dc9c88341aa39de00000000000000003f800000'
                   'bf8000003f000000400921fb544000003de0b4611a600000bfc5555555555555'
                   '3f811111111110b0bf2a01a01a013e1a3ec71de3a524f063be5ae6454b5dc0ab'
                   '3de6123c686ad430bd6ae420dc08499c3ce880ff6993df95'):
    raise ValueError('Observed table changed')
if records[1][1].hex() != 'fff8000000000000' or records[2][1].hex() != '3ff0000000000000':
    raise ValueError('Observed scalar constants changed')
get = lambda offset: struct.unpack_from('>d', table, offset)[0]
values = [get(8), get(40), get(48), get(16)]
coefficients = [1.0] + [get(offset) for offset in range(56, 120, 8)]
fixture = OUTPUT / 'constants_830d68f0.hpp'
byte_items = ',\n'.join(f'    std::pair<uint32_t,uint8_t>{{0x{address+i:08X},0x{byte:02X}}}'
                        for address, data in records for i, byte in enumerate(data))
fixture.write_text('#pragma once\n#include <utility>\n#include "acv_830d68f0.hpp"\n'
                   'inline constexpr acv_decomp::Constants830d68f0 constants_830d68f0{'
                   + ','.join(x.hex() for x in values) + ',{'
                   + ','.join(x.hex() for x in coefficients) + '}};\n'
                   f'inline constexpr std::array<std::pair<uint32_t,uint8_t>,136> constants_bytes_830d68f0{{{{\n'
                   + byte_items + '\n}};\n')
extracted = OUTPUT / 'generated_830d68f0.cpp'
cos_begin, cos_end = 0x830d69d0, 0x830d6ab0
cos_raw = (ROOT / 'tmp/scan/text.bin').read_bytes()[cos_begin-0x822b0000:cos_end-0x822b0000]
cos_digest = hashlib.sha256(cos_raw).hexdigest()
if len(cos_raw) != 224 or cos_digest != '0b963c9bc58eeae41bc9c9f44655b6673fd0f1060fe3a13599992a8979130a92':
    raise ValueError('Cosine raw boundary/hash mismatch')
cos_owners = [(b, b+((m >> 8) & 0x3fffff)*4)
              for b, m in struct.iter_unpack('>II', (ROOT / 'tmp/scan/pdata.bin').read_bytes())
              if b <= cos_begin < b+((m >> 8) & 0x3fffff)*4]
if cos_owners != [(cos_begin, cos_end)]:
    raise ValueError('Cosine .pdata mismatch')
cos_match = re.search(r'__attribute__\(\(alias\("__imp__sub_830D69D0"\)\)\) PPC_WEAK_FUNC\(sub_830D69D0\);\n'
                      r'PPC_FUNC_IMPL\(__imp__sub_830D69D0\) \{\n.*?\n\}', text, re.S)
if not cos_match:
    raise ValueError('Missing generated cosine')
cos_body = cos_match.group().split('{', 1)[1]
if any(x in cos_body for x in ('ERROR', '.long', 'PPC_CALL', 'sub_')):
    raise ValueError('Incomplete/non-leaf cosine')
if len(re.findall(r'^\t// ', cos_body, re.M)) != 56:
    raise ValueError('Cosine instruction count mismatch')
fixture.write_text(fixture.read_text() + f'inline constexpr double half_pi_830d69d0={get(0).hex()};\n')
extracted.write_text('#include "host.hpp"\n\n' + block + '\n' + cos_match.group() + '\n')
report = {
    'guest_start': '0x830D68F0', 'guest_end_exclusive': '0x830D69D0',
    'raw_size': len(raw), 'raw_sha256': digest, 'instruction_count': 56,
    'pdata_boundary_confirmed': True,
    'paired_function': {'guest_start': hex(cos_begin), 'end_exclusive': hex(cos_end),
                        'raw_sha256': cos_digest, 'size': 224, 'instruction_count': 56,
                        'pdata_boundary_confirmed': True},
    'generated_source_line': text[:match.start()].count('\n') + 1,
    'scope': 'finite-input return value, FE_TONEAREST, FTZ/DAZ off; generated split arithmetic comparison',
    'fused_candidate': 'std::fma experiment only; no Xenia/hardware proof',
    'tested_random_domain': [-1000000, 1000000],
    'excluded': ['NaN/infinity/large-input retail behavior', 'FPSCR/exceptions',
                 'scratch stack/volatile registers', 'retail matching', 'Windows execution'],
    'hashes': {},
}
for p in [source, GENERATED / 'ppc_context.h', GENERATED / 'ppc_config.h',
          ROOT / 'tools/native-decomp/acv_830d68f0.hpp',
          ROOT / 'tools/native-decomp/compare_830d68f0.cpp', Path(__file__),
          ROOT / 'tools/native-host-probe/host.hpp', extracted, fixture, *paths]:
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
                '-iquote', str(OUTPUT), '-I' + str(ROOT / 'tools/native-host-probe'),
                '-I' + str(ROOT / 'tools/native-decomp'), '-I' + str(GENERATED),
                '-I' + str(ROOT / 'tools/XenonRecomp/thirdparty/simde'),
                str(extracted), str(ROOT / 'tools/native-decomp/compare_830d68f0.cpp'),
                '-o', str(OUTPUT / 'compare')])
run('execution', [str(OUTPUT / 'compare')])
summary = json.loads(report['execution']['stdout'])
if summary['cases'] != 40392 or summary['split_mismatches'] != 0 or summary['cosine_split_mismatches'] != 0:
    raise ValueError('Unexpected test coverage/result')
