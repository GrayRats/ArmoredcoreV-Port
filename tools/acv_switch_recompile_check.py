#!/usr/bin/env python3
"""Regenerate into an isolated directory and verify the corrected ACV switch."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'logs/acv-switch-correction-20261020'
OUTPUT = ROOT / 'tmp/acv-switch-corrected-output'
if OUTPUT.exists():
    raise FileExistsError('Refusing to overwrite prior output: ' + str(OUTPUT))
# Upstream README requires the output directory to exist. SaveCurrentOutData
# calls fwrite on an unchecked fopen result if it does not (GDB evidence).
OUTPUT.mkdir(parents=True)
audit = json.loads((EVIDENCE / 'audit.json').read_text())
command = ['tmp/xenonrecomp-build/XenonRecomp/XenonRecomp',
           'tmp/acv-switch-corrected.toml', 'tools/XenonRecomp/XenonUtils/ppc_context.h']
start = time.monotonic()
with (EVIDENCE / 'recompile.stdout.log').open('w') as stdout, (EVIDENCE / 'recompile.stderr.log').open('w') as stderr:
    result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr)
(EVIDENCE / 'process.json').write_text(json.dumps({'command': command, 'exit_code': result.returncode,
    'duration_seconds': round(time.monotonic() - start, 3)}, indent=2) + '\n')
result.check_returncode()
log = (EVIDENCE / 'recompile.stdout.log').read_text()
stderr = (EVIDENCE / 'recompile.stderr.log').read_text()
found = []
for shard in sorted(OUTPUT.glob('ppc_recomp.*.cpp')):
    text = shard.read_text()
    match = re.search(r'PPC_FUNC_IMPL\(__imp__sub_82EF8620\) \{\n.*?\n\}', text, re.S)
    if match:
        found.append((shard, match.group()))
if len(found) != 1:
    raise ValueError('Expected exactly one generated dispatch owner')
shard, block = found[0]
switch = re.search(r'\tswitch \(ctx.r10.u64\) \{\n.*?\n\t\}', block, re.S)
if not switch:
    raise ValueError('Missing index-r10 switch in regenerated dispatch owner')
branches = re.findall(r'case (\d+):\s+goto loc_([0-9A-F]+);', switch.group())
expected = [(str(i), row['target'][2:]) for i, row in enumerate(audit['targets'])]
if branches != expected:
    raise ValueError('Generated cases disagree with audited ordered labels')
if any(f'loc_{target}:\n' not in block for _, target in branches):
    raise ValueError('Missing generated target label')
if 'ERROR' in switch.group() or 'PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32)' in block:
    raise ValueError('Dispatch still has a rejected or indirect switch path')
if 'Found a switch jump table at 82EF87B0' in log:
    raise ValueError('Missing-table diagnostic persists')
(EVIDENCE / 'generated-switch.cpp.txt').write_text(switch.group() + '\n')
report = {'command': command, 'exit_code': result.returncode,
          'duration_seconds': round(time.monotonic() - start, 3),
          'generated_shards': len(list(OUTPUT.glob('ppc_recomp.*.cpp'))),
          'source_shard': str(shard.relative_to(ROOT)),
          'verified_cases': len(branches), 'all_case_labels_emitted_in_owner': True,
          'missing_table_diagnostic': False,
          'unrecognized_instruction_messages': len(re.findall('Unrecognized instruction', log)),
          'unique_unrecognized_addresses': len(set(re.findall(r'Unrecognized instruction at 0x([0-9A-Fa-f]+)', log))),
          'error_messages': [line for line in (log + '\n' + stderr).splitlines() if 'ERROR' in line],
          'scope': 'generation verification only; no compilation/execution or ACV parity',
          'hashes': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in [ROOT / command[0], ROOT / command[1], EVIDENCE / 'switch.toml', shard]}}
(EVIDENCE / 'regeneration.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: report[k] for k in ('exit_code', 'duration_seconds', 'generated_shards', 'verified_cases',
                                       'unrecognized_instruction_messages', 'unique_unrecognized_addresses')}))
result.check_returncode()
if report['error_messages']:
    raise ValueError('Generator emitted error diagnostics; inspect saved report')
