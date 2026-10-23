#!/usr/bin/env python3
"""Extract two verbatim generated leaves, verify raw PPC words, compile and run.

Run through tools/decomp-logging/cli.mjs from the project root. No game boot.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--generated', default='tmp/xenon-out/ppc')
parser.add_argument('--output')
parser.add_argument('--target', choices=['linux', 'windows'], default='linux')
parser.add_argument('--wine', action='store_true', help='Run the Windows binary in an isolated Wine prefix under output/')
args = parser.parse_args()
if args.wine and args.target != 'windows':
    parser.error('--wine requires --target windows')
generated = (ROOT / args.generated).resolve()
output = (ROOT / (args.output or ('tmp/native-host-probe/windows' if args.target == 'windows'
                                else 'tmp/native-host-probe'))).resolve()
if not output.is_relative_to(ROOT / 'tmp'):
    raise ValueError('Output must be inside project tmp/')
output.mkdir(parents=True, exist_ok=True)
source = generated / 'ppc_recomp.0.cpp'
text = source.read_text()
raw = (ROOT / 'tmp/scan/text.bin').read_bytes()
blocks = []
evidence = []
for address, words in {
    0x822B0818: ['90830000', '4e800020'],
    0x822B0820: ['39600000', '91630000', '4e800020'],
}.items():
    offset = address - 0x822B0000
    actual = raw[offset:offset + len(words) * 4].hex()
    if actual != ''.join(words):
        raise ValueError(f'Raw PPC mismatch at {address:#x}')
    pattern = (rf'__attribute__\(\(alias\("__imp__sub_{address:08X}"\)\)\) '
               rf'PPC_WEAK_FUNC\(sub_{address:08X}\);\n'
               rf'PPC_FUNC_IMPL\(__imp__sub_{address:08X}\) \{{\n.*?\n\}}')
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f'Missing generated function {address:#x}')
    block = match.group()
    if 'ERROR' in block or '.long' in block or 'sub_' in block.split('{', 1)[1]:
        raise ValueError('Only complete call-free leaves are permitted')
    blocks.append(block)
    evidence.append({'guest_va': hex(address), 'ppc_words': words,
                     'source_line': text[:match.start()].count('\n') + 1,
                     'block_sha256': hashlib.sha256(block.encode()).hexdigest()})
extracted = output / 'generated_leaves.cpp'
extracted.write_text('#include "host.hpp"\n\n' + '\n\n'.join(blocks) + '\n')
report = {'scope': 'bounded ABI/memory probe; no kernel, graphics, XEX loader or game boot',
          'functions': evidence, 'inputs': {}}
for path in [source, generated / 'ppc_context.h', generated / 'ppc_config.h',
             ROOT / 'tmp/scan/text.bin', ROOT / 'tools/native-host-probe/host.hpp',
             ROOT / 'tools/native-host-probe/probe.cpp', Path(__file__)]:
    report['inputs'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()

def run(name, command, env=None, timeout=120):
    try:
        completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                   env=env, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        def decoded(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else (value or '')
        stdout, stderr = decoded(error.stdout), decoded(error.stderr)
        (output / f'{name}.stdout.txt').write_text(stdout)
        (output / f'{name}.stderr.txt').write_text(stderr)
        report[name] = {'command': list(map(str, command)), 'status': 'timeout',
                        'timeout_seconds': timeout, 'stdout': stdout, 'stderr': stderr}
        (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
        raise
    (output / f'{name}.stdout.txt').write_text(completed.stdout)
    (output / f'{name}.stderr.txt').write_text(completed.stderr)
    report[name] = {'command': list(map(str, command)), 'exit_code': completed.returncode,
                    'stdout': completed.stdout, 'stderr': completed.stderr}
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(completed.stdout, end='')
    print(completed.stderr, end='')
    completed.check_returncode()
    return completed

run('compiler_version', ['clang++', '--version'])
platform_flags = (['--target=x86_64-w64-windows-gnu', '-static', '-pthread'] if args.target == 'windows'
                  else ['-fsanitize=undefined', '-fno-sanitize-recover=all'])
binary = output / ('probe.exe' if args.target == 'windows' else 'probe')
report['target'] = args.target
report['execution_scope'] = 'Wine compatibility test' if args.wine else ('cross-build only' if args.target == 'windows' else 'native Linux with UBSan')
run('compile', ['clang++', '-std=c++20', '-O1', '-g', '-mavx2', '-fno-strict-aliasing',
                *platform_flags,
                '-I' + str(ROOT / 'tools/native-host-probe'), '-I' + str(generated),
                '-I' + str(ROOT / 'tools/XenonRecomp/thirdparty/simde'),
                str(extracted), str(ROOT / 'tools/native-host-probe/probe.cpp'),
                '-o', str(binary)])
report['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
if args.wine:
    run('wine_version', ['wine', '--version'])
    report['wine_environment'] = dict(WINEPREFIX=str(output / 'wine-prefix'), WINEDEBUG='-all',
                                      WINEARCH='win64', WINEDLLOVERRIDES='mscoree,mshtml=')
    env = dict(os.environ, **report['wine_environment'])
    execution = run('execution', ['wine', str(binary)], env=env)
elif args.target == 'linux':
    execution = run('execution', [str(binary)])
if args.wine or args.target == 'linux':
    observed = json.loads(execution.stdout)
    if observed != {'checks': 41, 'generated_functions': 2,
                    'guest_page': '0x83885000', 'status': 'passed'}:
        raise ValueError(f'Unexpected probe result: {observed}')
    report['validated_result'] = observed
(output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print('Evidence: ' + str(output / 'verification.json'))
