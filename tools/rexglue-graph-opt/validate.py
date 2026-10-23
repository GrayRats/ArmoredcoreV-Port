#!/usr/bin/env python3
"""Compare original/indexed SDK graph behavior and one synthetic workload."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
output = ROOT / 'tmp/rexglue-graph-opt'
report = {'scope': 'host SDK algorithm only; no PPC instruction/game behavior claim',
          'runs': {}, 'comparison': {}, 'inputs': {}}
provenance = json.loads((output / 'index-provenance.json').read_text())
for name, hashes in provenance['files'].items():
    for variant in ['baseline', 'indexed']:
        p = output / variant / name
        actual = hashlib.sha256(p.read_bytes()).hexdigest()
        if actual != hashes[variant + '_sha256']:
            raise ValueError('Prepared source changed: ' + str(p))
        report['inputs'][str(p.relative_to(ROOT))] = actual
for p in [Path(__file__), Path(__file__).with_name('prepare.py'),
          Path(__file__).with_name('graph_probe.cpp'), Path(__file__).with_name('CMakeLists.txt'),
          output / 'index-provenance.json', output / 'source-provenance.json',
          *[output / 'build' / ('graph_' + variant) for variant in ['baseline', 'indexed']]]:
    report['inputs'][str(p.relative_to(ROOT))] = hashlib.sha256(p.read_bytes()).hexdigest()

def save():
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')

for mode in ['trace', 'bench']:
    results = {}
    for variant in ['baseline', 'indexed']:
        command = [str(output / 'build' / ('graph_' + variant))] + (['bench'] if mode == 'bench' else [])
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        name = mode + '-' + variant
        (output / (name + '.stdout.txt')).write_text(result.stdout)
        (output / (name + '.stderr.txt')).write_text(result.stderr)
        report['runs'][name] = {'command': command, 'tool_exit_code': result.returncode,
                                'stdout': result.stdout, 'stderr': result.stderr}
        save()
        result.check_returncode()
        results[variant] = json.loads(result.stdout)
    comparable = lambda item: {key: value for key, value in item.items() if key != 'elapsed_us'}
    if comparable(results['baseline']) != comparable(results['indexed']):
        raise ValueError(f'Graph behavior digest mismatch for {mode}: {results}')
    report['comparison'][mode] = {'identical': comparable(results['baseline']),
                                 'observed_microseconds': {v: results[v]['elapsed_us'] for v in results}}
    save()
    print(mode, report['comparison'][mode])
print('Evidence:', output / 'verification.json')
