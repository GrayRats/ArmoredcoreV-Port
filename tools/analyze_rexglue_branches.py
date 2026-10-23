#!/usr/bin/env python3
"""Check saved ReXGlue unresolved branches against retail words and .pdata.

Read-only static evidence. A table extent is not a semantic function name and
does not by itself justify a new function or a particular codegen override.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from xex_read_guest_range import extract_image

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('diagnostics', type=Path)
parser.add_argument('--output', type=Path, default=Path('logs/acv-rexglue-unresolved-20261021'))
args = parser.parse_args()
output = args.output.resolve()
if not any(output.is_relative_to(ROOT / name) for name in ['logs', 'tmp']):
    parser.error('Output must be inside logs/ or tmp/')
output.mkdir(parents=True, exist_ok=True)
text_path = ROOT / 'tmp/scan/text.bin'
pdata_path = ROOT / 'tmp/scan/pdata.bin'
xex_path = ROOT / 'games/acv/extracted/default.xex'
text, pdata, xex = text_path.read_bytes(), pdata_path.read_bytes(), xex_path.read_bytes()
image, image_base, sections, _ = extract_image(xex)
if image_base != 0x82000000:
    raise ValueError('Unexpected image base')
if pdata != image[0x216A00:0x216A00 + len(pdata)]:
    raise ValueError('Saved .pdata differs from original mapped image')
functions = [(a, ((w >> 8) & 0x3fffff) * 4)
             for a, w in struct.iter_unpack('>II', pdata) if ((w >> 8) & 0x3fffff)]
owners = lambda address: [{'begin': hex(a), 'end': hex(a + size), 'length': size}
                          for a, size in functions if a <= address < a + size]
pairs = [(int(a, 16), int(b, 16)) for a, b in re.findall(
    r'^\s*0x([0-9A-Fa-f]+) from 0x([0-9A-Fa-f]+):', args.diagnostics.read_text(), re.M)]
if not pairs:
    raise ValueError('No branch diagnostics found')
records, targets = [], {}
for target, site in pairs:
    offset = site - 0x822B0000
    if not 0 <= offset <= len(text) - 4:
        raise ValueError('Branch site outside .text')
    word = struct.unpack_from('>I', text, offset)[0]
    if word >> 26 != 18 or word & 2:
        raise ValueError(f'Expected relative b/bl at {site:#x}, got {word:#x}')
    displacement = word & 0x03fffffc
    if displacement & 0x02000000:
        displacement -= 0x04000000
    if (site + displacement) & 0xffffffff != target:
        raise ValueError(f'Retail branch target differs at {site:#x}')
    if text[offset:offset+4] != image[site-image_base:site-image_base+4]:
        raise ValueError(f'Branch dump/XEX mismatch at {site:#x}')
    target_offset = target - 0x822B0000
    if not 0 <= target_offset <= len(text) - 16:
        raise ValueError('Target outside .text')
    if text[target_offset:target_offset+16] != image[target-image_base:target-image_base+16]:
        raise ValueError(f'Target dump/XEX mismatch at {target:#x}')
    site_owners, target_owners = owners(site), owners(target)
    records.append({'site': hex(site), 'target': hex(target), 'word': f'{word:08x}',
                    'link': bool(word & 1), 'site_pdata_owners': site_owners,
                    'target_pdata_owners': target_owners,
                    'shared_pdata_owner': bool({f['begin'] for f in site_owners} &
                                              {f['begin'] for f in target_owners})})
    targets.setdefault(target, {'target': hex(target), 'pdata_owners': target_owners, 'sites': []})['sites'].append(hex(site))
for target, item in sorted(targets.items()):
    start, end = max(0x822B0000, target-16), target+64
    result = subprocess.run(['python3', str(ROOT / 'tools/ppc_dis.py'), str(text_path),
                             hex(start), hex(end)], capture_output=True, text=True, check=True)
    filename = f'raw-target-{target:08x}.asm'
    (output / filename).write_text(result.stdout)
    item['raw_window'] = filename
inputs = [args.diagnostics, text_path, pdata_path, xex_path, Path(__file__),
          ROOT / 'tools/xex_read_guest_range.py', ROOT / 'tools/ppc_dis.py']
report = {'confidence': 'confirmed raw relative branches and table membership; semantics/codegen fix unverified',
          'branch_count': len(records), 'unique_targets': len(targets),
          'shared_pdata_owner_pairs': sum(r['shared_pdata_owner'] for r in records),
          'target_without_pdata_pairs': sum(not r['target_pdata_owners'] for r in records),
          'records': records, 'targets': [item for _, item in sorted(targets.items())],
          'inputs': {str(p.resolve().relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in inputs}}
(output / 'retail-branch-inventory.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: report[k] for k in ['branch_count', 'unique_targets', 'shared_pdata_owner_pairs',
                                        'target_without_pdata_pairs']}, indent=2))
print('Evidence:', output / 'retail-branch-inventory.json')
