#!/usr/bin/env python3
"""Verify and execute the complete generated 822C8AD8 -> 82B8F9C0 closure.

The only host injection is a deterministic source-level __rdtsc replacement.
This establishes neither Xenon time-base frequency nor retail runtime parity.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', choices=['linux', 'windows'], default='linux')
parser.add_argument('--wine', action='store_true')
parser.add_argument('--frequency-import', action='store_true',
                    help='Probe 82B8FA70 with an explicit configured frequency adapter')
args = parser.parse_args()
if args.wine and args.target != 'windows':
    parser.error('--wine requires --target windows')
output = ROOT / ('tmp/native-host-frequency' if args.frequency_import
                 else 'tmp/native-host-clock-closure') / args.target
output.mkdir(parents=True, exist_ok=True)
generated = ROOT / 'tmp/xenon-more-mulhd-output'
sources = [(0x822C8AD8, 5, '7d8802a69181fff89421ffa038610050488c6ed9e8610050382100608181fff87d8803a64e800020'),
           (0x82B8F9C0, 490, '7d6c42e6556a003f408200087d6c42e6f9630000386000014e800020')]
if args.frequency_import:
    sources = [(0x82B8FA70, 490, '7d8802a69181fff8fbe1fff09421ffa07c7f1b78489a7221f87f000038600001382100608181fff87d8803a6ebe1fff04e800020')]
raw_path = ROOT / 'tmp/scan/text.bin'
pdata_path = ROOT / 'tmp/scan/pdata.bin'
raw = raw_path.read_bytes()
wrapper = 0x82B8FA70 if args.frequency_import else 0x822C8AD8
entries = [(a, ((w >> 8) & 0x3fffff) * 4)
           for a, w in struct.iter_unpack('>II', pdata_path.read_bytes()) if a == wrapper]
if entries != [(wrapper, 52 if args.frequency_import else 40)]:
    raise ValueError('Wrapper .pdata boundary differs from the checked extent')
inputs = [raw_path, pdata_path, Path(__file__),
          ROOT / 'tools/native-host-probe/host.hpp',
          ROOT / 'tools/native-host-probe' / ('frequency_probe.cpp' if args.frequency_import
                                             else 'clock_closure_probe.cpp'),
          generated / 'ppc_context.h', generated / 'ppc_config.h']
report = {'scope': ('one generated wrapper plus configurable u32 host import; actual Xenon frequency and parity unverified'
                    if args.frequency_import else __doc__), 'target': args.target, 'functions': [],
          'execution_scope': 'Wine compatibility test' if args.wine else
          ('cross-build only' if args.target == 'windows' else 'native Linux with UBSan')}
blocks = []
for address, shard, expected in sources:
    source = generated / f'ppc_recomp.{shard}.cpp'
    inputs.append(source)
    text = source.read_text()
    offset = address - 0x822B0000
    if raw[offset:offset + len(expected) // 2].hex() != expected:
        raise ValueError(f'PPC mismatch at {address:#x}')
    pattern = (rf'__attribute__\(\(alias\("__imp__sub_{address:08X}"\)\)\) '
               rf'PPC_WEAK_FUNC\(sub_{address:08X}\);\n'
               rf'PPC_FUNC_IMPL\(__imp__sub_{address:08X}\) \{{\n.*?\n\}}')
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f'Missing generated function at {address:#x}')
    block = match.group()
    if 'ERROR' in block or 'PPC_CALL_INDIRECT_FUNC' in block:
        raise ValueError('Incomplete or indirect generated closure')
    callees = set(re.findall(r'\bsub_([0-9A-F]{8})\(ctx, base\)', block))
    if callees != ({'82B8F9C0'} if address == 0x822C8AD8 else set()):
        raise ValueError('Unexpected generated dependency')
    imports = set(re.findall(r'\b__imp__(\w+)\(ctx, base\)', block))
    if imports != ({'KeQueryPerformanceFrequency'} if args.frequency_import else set()):
        raise ValueError('Unexpected generated import dependency')
    blocks.append(block)
    report['functions'].append({'va': hex(address), 'words': expected,
                                'source': str(source.relative_to(ROOT)),
                                'line': text[:match.start()].count('\n') + 1,
                                'block_sha256': hashlib.sha256(block.encode()).hexdigest()})
extracted = output / 'generated_closure.cpp'
if args.frequency_import:
    prelude = '#include "frequency_service.hpp"\n'
    inputs.extend(ROOT / 'tools/native-host-probe' / name for name in
                  ['frequency_service.hpp', 'frequency_service.cpp'])
    inventory = ROOT / 'logs/acv-host-20261021/runtime-inventory.json'
    inputs.append(inventory)
    records = json.loads(inventory.read_text())['libraries']
    matched = [i for lib in records if lib['name'] == 'xboxkrnl.exe' for i in lib['imports']
               if i['name'] == 'KeQueryPerformanceFrequency']
    if len(matched) != 1 or matched[0]['ordinal'] != 131 or matched[0]['thunk_slots'] != ['0x83536ca4']:
        raise ValueError('Frequency import identity differs from inventory')
    report['import_policy'] = 'explicit configured u32; no default; reference ABI from xboxkrnl_threading.cpp:371-374'
else:
    prelude = ('#include "host.hpp"\nPPC_EXTERN_FUNC(sub_82B8F9C0);\n'
               'uint64_t mock_timebase_read();\n#define __rdtsc() mock_timebase_read()\n')
extracted.write_text(prelude + '\n\n'.join(blocks) + '\n#undef __rdtsc\n')
report['inputs'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}

def save():
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')

def run(name, command, env=None):
    try:
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True, timeout=120)
    except subprocess.TimeoutExpired as error:
        report[name] = {'command': command, 'status': 'timeout', 'timeout_seconds': 120}
        for stream in ['stdout', 'stderr']:
            value = getattr(error, stream) or b''
            value = value.decode(errors='replace') if isinstance(value, bytes) else value
            (output / f'{name}.{stream}.txt').write_text(value)
        save()
        raise
    for stream in ['stdout', 'stderr']:
        (output / f'{name}.{stream}.txt').write_text(getattr(result, stream))
    report[name] = {'command': command, 'exit_code': result.returncode,
                    'stdout': result.stdout, 'stderr': result.stderr}
    save()
    print(result.stdout, end='')
    print(result.stderr, end='')
    result.check_returncode()
    return result

run('compiler_version', ['clang++', '--version'])
flags = ['--target=x86_64-w64-windows-gnu', '-static', '-pthread'] if args.target == 'windows' else [
    '-fsanitize=undefined', '-fno-sanitize-recover=all']
binary = output / ('clock_closure.exe' if args.target == 'windows' else 'clock_closure')
run('compile', ['clang++', '-std=c++20', '-O1', '-g', '-mavx2', '-fno-strict-aliasing', *flags,
                '-I' + str(ROOT / 'tools/native-host-probe'), '-I' + str(generated),
                str(extracted), str(ROOT / 'tools/native-host-probe' / ('frequency_probe.cpp' if args.frequency_import else 'clock_closure_probe.cpp')),
                *([str(ROOT / 'tools/native-host-probe/frequency_service.cpp')] if args.frequency_import else []),
                '-o', str(binary)])
report['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
if args.wine:
    run('wine_version', ['wine', '--version'])
    report['wine_environment'] = dict(WINEPREFIX=str(output / 'wine-prefix'), WINEDEBUG='-all',
                                      WINEARCH='win64', WINEDLLOVERRIDES='mscoree,mshtml=')
    execution = run('execution', ['wine', str(binary)], dict(os.environ, **report['wine_environment']))
elif args.target == 'linux':
    execution = run('execution', [str(binary)])
if args.wine or args.target == 'linux':
    observed = json.loads(execution.stdout)
    expected = ({'checks': 44, 'cases': 5, 'generated_functions': 1, 'imported_services': 1, 'status': 'passed'}
                if args.frequency_import else
                {'checks': 32768, 'cases': 4096, 'generated_functions': 2, 'status': 'passed'})
    if observed != expected:
        raise ValueError(f'Unexpected result: {observed}')
    report['validated_result'] = observed
save()
print('Evidence:', output / 'verification.json')
