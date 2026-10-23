#!/usr/bin/env python3
"""Evaluate pinned upstream instruction branch without altering main checkout."""
import hashlib
import argparse
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'tools/XenonRecomp'
COMMIT = '9ff80d8321063657ea0e44edcc7ae6a60532e2c1'
SOURCE = ROOT / 'tmp/xenon-more-source'
BUILD = ROOT / 'tmp/xenon-more-build'
OUTPUT = ROOT / 'tmp/xenon-more-output'
LOGS = ROOT / 'logs/acv-more-instructions-20261020'


def command(args, cwd=ROOT):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-build-fix', action='store_true',
                        help='Apply main-compatible lzxDecompress linkage fix in isolated source and resume')
    resume = parser.parse_args().resume_build_fix
    for p in (SOURCE, BUILD, OUTPUT):
        if p.exists() and not resume:
            raise FileExistsError(f'Refusing to overwrite {p}')
    LOGS.mkdir(parents=True, exist_ok=True)
    report = {'commit': COMMIT, 'main_commit': command(['git', 'rev-parse', 'HEAD'], UPSTREAM),
              'submodules': {}, 'scope': 'generation coverage only; no semantic validation'}
    pins = command(['git', 'ls-tree', '-r', COMMIT], UPSTREAM).splitlines()
    for row in pins:
        metadata, name = row.split('\t', 1)
        if not metadata.startswith('160000 '):
            continue
        pin = metadata.split()[2]
        actual = command(['git', 'rev-parse', 'HEAD'], UPSTREAM / name)
        if actual != pin or command(['git', 'status', '--porcelain'], UPSTREAM / name):
            raise ValueError(f'Submodule mismatch or modified source: {name}')
        report['submodules'][name] = pin

    def unpack(repo, revision, target):
        target.mkdir(parents=True, exist_ok=True)
        data = subprocess.check_output(['git', 'archive', revision], cwd=repo)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            archive.extractall(target, filter='data')

    if not resume:
        unpack(UPSTREAM, COMMIT, SOURCE)
        for name, pin in report['submodules'].items():
            unpack(UPSTREAM / name, pin, SOURCE / name)
        OUTPUT.mkdir()
    else:
        report['initial_attempt'] = json.loads((LOGS / 'verification.json').read_text())
        patch_file = SOURCE / 'XenonUtils/xex_patcher.cpp'
        original = patch_file.read_text()
        replacement = original.replace('static int lzxDecompress(', 'int lzxDecompress(')
        if replacement == original:
            raise ValueError('Expected exact isolated branch linkage conflict')
        patch_file.write_text(replacement)
        report['build_fix'] = {'file': 'XenonUtils/xex_patcher.cpp',
          'change': 'remove static from lzxDecompress definition to agree with extern header; same linkage as main',
          'sha256': hashlib.sha256(patch_file.read_bytes()).hexdigest()}
        (LOGS / 'build-fix.patch').write_text(subprocess.check_output(
            ['git', 'diff', COMMIT, 'HEAD', '--', 'XenonUtils/xex_patcher.cpp'], cwd=UPSTREAM, text=True))

    def run(stage, args):
        start = time.monotonic()
        with (LOGS / f'{stage}.stdout.log').open('w') as out, (LOGS / f'{stage}.stderr.log').open('w') as err:
            result = subprocess.run(args, cwd=ROOT, stdout=out, stderr=err)
        report[stage] = {'command': list(map(str, args)), 'exit_code': result.returncode,
                         'seconds': round(time.monotonic() - start, 3)}
        (LOGS / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
        result.check_returncode()

    if not resume:
        run('configure', ['cmake', '-S', str(SOURCE), '-B', str(BUILD), '-G', 'Ninja',
                          '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++'])
    run('build_recovery' if resume else 'build', ['cmake', '--build', str(BUILD), '--target', 'XenonRecomp', '-j', '4'])
    config = (ROOT / 'tmp/acv-switch-corrected.toml').read_text().replace('acv-switch-corrected-output', 'xenon-more-output')
    config_path = ROOT / 'tmp/acv-more-instructions.toml'
    config_path.write_text(config)
    binary = BUILD / 'XenonRecomp/XenonRecomp'
    report['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    run('recompile', [str(binary), str(config_path), str(SOURCE / 'XenonUtils/ppc_context.h')])
    old = (ROOT / 'logs/acv-switch-correction-20261020/recompile.stdout.log').read_text()
    new = (LOGS / 'recompile.stdout.log').read_text()
    pattern = r'Unrecognized instruction at 0x([0-9A-Fa-f]+): (\S+)'
    old_pairs = re.findall(pattern, old)
    new_pairs = re.findall(pattern, new)
    old_addresses = {a for a, _ in old_pairs}
    new_addresses = {a for a, _ in new_pairs}
    from collections import Counter
    report['coverage'] = {'before_messages': len(old_pairs), 'after_messages': len(new_pairs),
         'before_unique': len(old_addresses), 'after_unique': len(new_addresses),
         'removed_unique': len(old_addresses - new_addresses), 'new_unique': len(new_addresses - old_addresses),
         'remaining_mnemonics': dict(Counter(m for _, m in new_pairs)),
         'error_messages': [s for s in new.splitlines() if 'ERROR' in s],
         'missing_corrected_switch': 'Found a switch jump table at 82EF87B0' in new,
         'generated_shards': len(list(OUTPUT.glob('ppc_recomp.*.cpp')))}
    (LOGS / 'remaining-addresses.json').write_text(json.dumps(new_pairs, indent=2) + '\n')
    (LOGS / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['coverage']))
    if report['coverage']['error_messages'] or report['coverage']['missing_corrected_switch']:
        raise ValueError('Generator diagnostics remain; inspect report')


if __name__ == '__main__':
    main()
