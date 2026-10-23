#!/usr/bin/env python3
"""Read-only XEX runtime metadata/import inventory from unpatched image words.

Format sources: XenonUtils/xex.h and xex.cpp:296-345. Export names come from
the installed XenonUtils/xbox/*.inc tables, not the 360tools ordinal dictionary.
No service implementation, executable loading or runtime compatibility is claimed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from xex_read_guest_range import extract_image

ROOT = Path(__file__).resolve().parents[1]


def span(data, offset, length):
    if offset < 0 or length < 0 or offset > len(data) or length > len(data) - offset:
        raise ValueError(f'Out-of-range file span: {offset:#x}+{length:#x}')
    return data[offset:offset + length]


def headers(data):
    span(data, 0, 24)
    magic, flags, size, reserved, security, count = struct.unpack_from('>6I', data)
    if magic != 0x58455832 or size > len(data) or size < 24:
        raise ValueError('Invalid XEX2 header')
    span(data[:size], 24, count * 8)
    result = {}
    for i in range(count):
        key, value = struct.unpack_from('>II', data, 24 + i * 8)
        if key in result:
            raise ValueError('Duplicate XEX optional header')
        result[key] = value
    return result, size


def guest_word(image, base, sections, address):
    if address & 3 or not any(base + s['rva'] <= address and
                              address + 4 <= base + s['rva'] + s['virtual_size']
                              for s in sections):
        raise ValueError(f'Unmapped/misaligned import record VA {address:#x}')
    return struct.unpack('>I', span(image, address - base, 4))[0]


def export_tables(directory):
    result, paths = {}, []
    for name, filename in [('xam.xex', 'xam_table.inc'), ('xboxkrnl.exe', 'xboxkrnl_table.inc')]:
        path = directory / filename
        paths.append(path)
        matches = re.findall(r'^XE_EXPORT\(\w+,\s*(0x[0-9A-Fa-f]+),\s*(\w+),\s*(k\w+)\)',
                             path.read_text(), re.M)
        if not matches:
            raise ValueError(f'No export declarations in {path}')
        result[name] = {int(ordinal, 16): {'name': symbol, 'export_kind': kind}
                        for ordinal, symbol, kind in matches}
    return result, paths


def parse_imports(data, offset, image, base, sections, names):
    size, strings_size, string_count = struct.unpack('>III', span(data, offset, 12))
    block = span(data, offset, size)
    strings = span(block, 12, strings_size)
    table, cursor = [], 0
    for _ in range(string_count):
        end = strings.find(b'\0', cursor)
        if end < 0:
            raise ValueError('Unterminated import library string')
        table.append(strings[cursor:end].decode('ascii'))
        cursor = (end + 4) & ~3
        if cursor > strings_size:
            raise ValueError('Import string padding exceeds table')
    libraries, cursor = [], 12 + strings_size
    while cursor < len(block):
        library_header = span(block, cursor, 40)
        library_size = struct.unpack_from('>I', library_header)[0]
        if library_size == 0 and not any(block[cursor:]):
            break
        name_index, count = struct.unpack_from('>HH', library_header, 36)
        if library_size != 40 + count * 4:
            raise ValueError('Import library size/count mismatch')
        library = span(block, cursor, library_size)
        index = name_index & 0xff
        if index >= len(table):
            raise ValueError('Import library name index outside string table')
        name = table[index]
        records, grouped = [], {}
        for i in range(count):
            address = struct.unpack_from('>I', library, 40 + i * 4)[0]
            if address == 0:
                records.append({'descriptor_offset': hex(offset + cursor + 40 + i * 4),
                                'record_va': '0x0', 'status': 'null descriptor'})
                continue
            value = guest_word(image, base, sections, address)
            kind, ordinal = value >> 24, value & 0xffff
            if kind not in (0, 1):
                raise ValueError(f'Unsupported import record type {kind} at {address:#x}')
            record = {'descriptor_offset': hex(offset + cursor + 40 + i * 4),
                      'record_va': hex(address), 'raw_word': hex(value),
                      'record_type': kind, 'hint': (value >> 16) & 0xff, 'ordinal': ordinal}
            records.append(record)
            item = grouped.setdefault(ordinal, {'ordinal': ordinal,
                                               **names.get(name, {}).get(ordinal, {}),
                                               'value_slots': [], 'thunk_slots': []})
            item['thunk_slots' if kind else 'value_slots'].append(hex(address))
        libraries.append({'name': name, 'header_offset': hex(offset + cursor),
                          'version_raw': hex(struct.unpack_from('>I', library, 28)[0]),
                          'record_count': count, 'records': records,
                          'imports': sorted(grouped.values(), key=lambda item: item['ordinal'])})
        cursor += library_size
    return libraries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xex', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(output.is_relative_to(ROOT / directory) for directory in ['logs', 'tmp']):
        parser.error('Output must be inside project logs/ or tmp/')
    data = args.xex.read_bytes()
    options, header_size = headers(data)
    image, base, sections, extraction = extract_image(data)
    names, name_paths = export_tables(ROOT / 'tools/XenonRecomp/XenonUtils/xbox')
    if 0x103ff not in options:
        raise ValueError('XEX import library header is missing')
    libraries = parse_imports(data[:header_size], options[0x103ff], image, base, sections, names)
    metadata = {name: hex(options[key]) for key, name in [
        (0x10100, 'entry_point'), (0x10201, 'image_base'),
        (0x20200, 'default_stack_size'), (0x20401, 'default_heap_size')]
        if key in options}
    if 0x20104 in options:
        tls = struct.unpack('>4I', span(data[:header_size], options[0x20104], 16))
        metadata['tls'] = dict(zip(['slot_count', 'raw_data_address', 'data_size', 'raw_data_size'],
                                  [tls[0], hex(tls[1]), tls[2], tls[3]]))
    inputs = [args.xex, Path(__file__), ROOT / 'tools/xex_read_guest_range.py', *name_paths,
              ROOT / 'tools/XenonRecomp/XenonUtils/xex.h', ROOT / 'tools/XenonRecomp/XenonUtils/xex.cpp']
    report = {'confidence': 'confirmed input bytes and source-table name mapping; no service semantics',
              'xex_sha256': hashlib.sha256(data).hexdigest(), 'metadata': metadata,
              'image_base': hex(base), 'image_size': len(image), 'extraction': extraction,
              'sections': sections, 'libraries': libraries,
              'inputs': {str(p.resolve().relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in inputs}}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'metadata': metadata, 'libraries': [
        {'name': lib['name'], 'records': lib['record_count'], 'imports': len(lib['imports']),
         'function_exports': sum(i.get('export_kind') == 'kFunction' for i in lib['imports']),
         'variable_exports': sum(i.get('export_kind') == 'kVariable' for i in lib['imports']),
         'unmapped_ordinals': sum('name' not in i for i in lib['imports'])} for lib in libraries],
        'output': str(output)}, indent=2))


if __name__ == '__main__':
    main()
