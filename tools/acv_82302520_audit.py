#!/usr/bin/env python3
"""Read-only callee inventory; cached Xenia decode fragments are partial evidence."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from acv_xenia_opcode_audit import classify, extract_bits

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'logs/acv-callee-20261021'
START, END, BASE = 0x82302520, 0x82302918, 0x822B0000


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    text = (ROOT / 'tmp/scan/text.bin').read_bytes()
    pdata = (ROOT / 'tmp/scan/pdata.bin').read_bytes()
    raw = text[START-BASE:END-BASE]
    if hashlib.sha256(raw).hexdigest() != '2230c81445052926bfce897bf0352562a1901a690b19ed168cf4a59477627979':
        raise ValueError('Unexpected original function bytes')
    owners = [(b, b+((m >> 8) & 0x3fffff)*4)
              for b, m in struct.iter_unpack('>II', pdata)
              if b <= START < b+((m >> 8) & 0x3fffff)*4]
    if owners != [(START, END)]:
        raise ValueError('Unexpected .pdata extent')
    generated_path = ROOT / 'tmp/xenon-out/ppc/ppc_recomp.17.cpp'
    generated = generated_path.read_text()
    match = re.search(r'PPC_FUNC_IMPL\(__imp__sub_82302520\) \{\n.*?\n\}', generated, re.S)
    if not match:
        raise ValueError('Missing generated body')
    annotations = re.findall(r'^\t// (.*)$', match.group(), re.M)
    if len(annotations) != len(raw)//4:
        raise ValueError('Generated comment alignment cannot be established')
    llvm = subprocess.run(['python3', 'tools/ppc_dis.py', 'tmp/scan/text.bin',
                           hex(START), hex(END)], cwd=ROOT, capture_output=True,
                          text=True, check=True).stdout
    lines = llvm.splitlines()
    if len(lines) != len(annotations):
        raise ValueError('Raw listing length mismatch')
    cached_path = ROOT / 'logs/acv-dialect-audit-20261020/xenia-decode-audit.json'
    cached = json.loads(cached_path.read_text())
    tables = {int(k): v for k, v in cached['lookup_fragments'].items()}
    rows, calls, histogram = [], [], Counter()
    for i, (word,) in enumerate(struct.iter_unpack('>I', raw)):
        address = START+4*i
        primary = extract_bits(word, 0, 5)
        row = {'va': hex(address), 'raw_word': f'{word:08x}',
               'llvm_pwr7_display': lines[i], 'generated_annotation': annotations[i]}
        if primary in (4, 5):
            name, trace = classify(word, tables)
            if name is None:
                raise ValueError(f'No cached decoder hit at {address:#x}')
            row['xenia_decoder_name'] = name
            row['ordered_lookup_trace'] = trace
            histogram[name] += 1
        elif primary == 6:
            row['decode_limit'] = 'Primary 6 fragments not present in cached audit; generated annotation is not independent proof'
        # Direct relative bl, decoded from the original instruction word.
        if primary == 18 and word & 1 and not word & 2:
            displacement = word & 0x03FFFFFC
            if displacement & 0x02000000:
                displacement -= 0x04000000
            calls.append({'site': hex(address), 'target': hex((address+displacement) & 0xFFFFFFFF)})
        rows.append(row)
    half = json.loads((OUT / 'constant-820d8d54.json').read_text())
    if not half['ok'] or half['result']['hex'] != '3f000000':
        raise ValueError('Unexpected loaded-image half constant')
    report = {'scope': 'static instruction/dataflow inventory; not original execution or complete semantic reconstruction',
              'start': hex(START), 'end_exclusive': hex(END), 'size': len(raw),
              'instruction_count': len(rows), 'pdata_boundary_confirmed': True,
              'raw_sha256': hashlib.sha256(raw).hexdigest(),
              'text_sha256': hashlib.sha256(text).hexdigest(),
              'pdata_sha256': hashlib.sha256(pdata).hexdigest(),
              'generated_path': str(generated_path.relative_to(ROOT)),
              'generated_sha256': hashlib.sha256(generated_path.read_bytes()).hexdigest(),
              'direct_calls': calls,
              'cached_xenia_source_commit': cached['source_commit'],
              'cached_xenia_audit_sha256': hashlib.sha256(cached_path.read_bytes()).hexdigest(),
              'independently_classified_primary_4_5': dict(histogram),
              'not_covered_primary_6_words': sum(int(x['raw_word'], 16) >> 26 == 6 for x in rows),
              'confirmed_scalar_prefix': {'dimension_storage': ['+0x4C', '+0x50'],
                  'mode_word': '+0x1D0 = 0',
                  'ratio_word': '+0xC0 = float(float(uint32(r8))/float(uint32(r9)))',
                  'three_call_inputs_f1': 'float(input_f1 * 0.5f) at 0x82302590/98/AC/B8'},
              'instructions': rows}
    (OUT / 'callee-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    (OUT / 'callee-annotated.txt').write_text('\n'.join(
        f'{r["va"]} {r["raw_word"]} | generated: {r["generated_annotation"]}'
        + (f' | Xenia source: {r["xenia_decoder_name"]}' if 'xenia_decoder_name' in r else '')
        for r in rows)+'\n')
    print(json.dumps({k: report[k] for k in ('start', 'size', 'instruction_count', 'direct_calls',
        'independently_classified_primary_4_5', 'not_covered_primary_6_words')}))


if __name__ == '__main__':
    main()
