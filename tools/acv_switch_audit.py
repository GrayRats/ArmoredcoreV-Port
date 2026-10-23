#!/usr/bin/env python3
"""Read-only ACV 0x82EF87B0 switch audit; write evidence and XenonRecomp TOML.

All input data is preserved. This evaluator verifies the exact dispatch opcodes,
models signed addis/addi plus unsigned lhzx offsets, and checks every .pdata owner.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
TEXT_BASE = 0x822B0000


def signed16(value):
    return value - 0x10000 if value & 0x8000 else value


def lis_addi(high, low):
    return ((signed16(high & 0xffff) << 16) + signed16(low & 0xffff)) & 0xffffffff


def audit(text, pdata, table):
    def word(address):
        offset = address - TEXT_BASE
        if offset < 0 or offset + 4 > len(text):
            raise ValueError(f'Address outside text: {address:#x}')
        return struct.unpack_from('>I', text, offset)[0]

    expected = {
        0x82EF869C: 0x2B0A001C,  # cmplwi cr6,r10,28
        0x82EF86B4: 0x41991404,  # bgt cr6,0x82EF9AB8 (default/error path)
        0x82EF878C: 0x3D808215,  # lis r12,-32235
        0x82EF8790: 0x398C2100,  # addi r12,r12,8448
        0x82EF8794: 0x5540083C,  # slwi r0,r10,1
        0x82EF8798: 0x7C0C022E,  # lhzx r0,r12,r0
        0x82EF879C: 0x3D8082F0,  # lis r12,-32016
        0x82EF87A0: 0x398C87B4,  # addi r12,r12,-30796
        0x82EF87A4: 0x7D8C0214,  # add r12,r12,r0
        0x82EF87A8: 0x7D8903A6,  # mtctr r12
        0x82EF87AC: 0x60000000,
        0x82EF87B0: 0x4E800420,
    }
    for address, opcode in expected.items():
        if word(address) != opcode:
            raise ValueError(f'Unexpected instruction at {address:#x}: {word(address):08x}, expected {opcode:08x}')
    count = (word(0x82EF869C) & 0xffff) + 1
    if len(table) != count * 2:
        raise ValueError('Table length disagrees with index bound')
    if len(pdata) % 8:
        raise ValueError('Partial .pdata record')
    bounds = []
    for start, metadata in struct.iter_unpack('>II', pdata):
        length = ((metadata >> 8) & 0x3fffff) * 4
        if length:
            bounds.append((start, start + length))
    owners = lambda address: [(start, end) for start, end in bounds if start <= address < end]
    dispatch_owners = owners(0x82EF87B0)
    if dispatch_owners != [(0x82EF8620, 0x82EF9C58)]:
        raise ValueError(f'Unexpected dispatch owner: {dispatch_owners}')
    target_base = lis_addi(word(0x82EF879C), word(0x82EF87A0))
    table_base = lis_addi(word(0x82EF878C), word(0x82EF8790))
    rows = []
    for index, (offset,) in enumerate(struct.iter_unpack('>H', table)):
        target = (target_base + offset) & 0xffffffff
        row = {'index': index, 'unsigned_halfword': offset, 'target': f'0x{target:08X}',
               'aligned': target % 4 == 0, 'inside_text': TEXT_BASE <= target < TEXT_BASE + len(text),
               'same_pdata_owner': owners(target) == dispatch_owners,
               'first_word': f'{word(target):08X}'}
        if not all(row[key] for key in ('aligned', 'inside_text', 'same_pdata_owner')):
            raise ValueError(f'Invalid switch target: {row}')
        rows.append(row)
    return {'switch_pc': '0x82EF87B0', 'index_register': 10, 'index_count': count,
            'table_base': f'0x{table_base:08X}', 'target_base': f'0x{target_base:08X}',
            'arithmetic': '((-32016 << 16) + (-30796)) & 0xffffffff = 0x82EF87B4',
            'supersedes': 'Old 0x82F087B4 base incorrectly treated addi immediate as unsigned',
            'dispatch_owner': {'start': '0x82EF8620', 'end_exclusive': '0x82EF9C58'},
            'instructions': {f'0x{a:08X}': f'{word(a):08X}' for a in expected}, 'targets': rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'logs/acv-switch-correction-20261020')
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(output.is_relative_to(ROOT / directory) for directory in ('tmp', 'logs')):
        raise ValueError('Output must stay under project tmp/ or logs/')
    inputs = [ROOT / 'tmp/scan/text.bin', ROOT / 'tmp/scan/pdata.bin',
              ROOT / 'logs/xenonrecomp-followup-20261020/switch-table-82152100.bin']
    data = [p.read_bytes() for p in inputs]
    report = audit(*data)
    report['inputs'] = {str(p.relative_to(ROOT)): hashlib.sha256(b).hexdigest() for p, b in zip(inputs, data)}
    output.mkdir(parents=True, exist_ok=True)
    (output / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    labels = ',\n'.join('    ' + row['target'] for row in report['targets'])
    (output / 'switch.toml').write_text('# base is the dispatch instruction PC, not table data VA.\n'
        '[[switch]]\nbase = 0x82EF87B0\nr = 10\nlabels = [\n' + labels + '\n]\n')
    print(json.dumps({k: report[k] for k in ('switch_pc', 'index_count', 'table_base', 'target_base', 'dispatch_owner')}))
    print(f'All {len(report["targets"])} aligned targets share the dispatch .pdata owner. Evidence: {output}')


if __name__ == '__main__':
    main()
