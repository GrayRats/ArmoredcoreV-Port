#!/usr/bin/env python3
"""Read-only classification of ACV's 38 reports using Xenia lookup fragments.

Downloads official source into memory. Writes only the bounded audit JSON; it
does not decode operands, implement instructions, or execute guest instructions.
"""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / 'logs/acv-more-instructions-20261020/remaining-instructions.json'
OUTPUT = ROOT / 'logs/acv-dialect-audit-20261020/xenia-decode-audit.json'
REPOSITORY = 'https://github.com/xenia-project/xenia'
SOURCE_PATH = 'src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc'
EXPECTED = {'frsqrte': 'frsqrtex', 'vnor128': 'vnor128', 'mullhwu.': 'vsldoi128',
            'lhbrx': 'lhbrx', 'mulhd': 'mulhdx'}


def fetch(url):
    with urlopen(Request(url, headers={'User-Agent': 'ACV-read-only-opcode-audit'}), timeout=30) as response:
        return response.read()


def extract_bits(word, first, last):
    # Xenia bit numbering: bit 0 is the MSB, bit 31 is the LSB.
    return (word >> (32 - 1 - last)) & ((1 << (last - first + 1)) - 1)


def fragments(source, primary):
    """Parse only the ordered flat lookup switches for one primary opcode.

    All case alternatives in every relevant fragment are retained. A strict
    grammar check fails if upstream introduces unsupported lookup syntax.
    """
    branches = list(re.finditer(r'^  case (\d+):', source, re.M))
    position = next(i for i, match in enumerate(branches) if int(match[1]) == primary)
    start = branches[position].end()
    end = branches[position + 1].start() if position + 1 < len(branches) else len(source)
    body = source[start:end]
    switches = list(re.finditer(r'^    switch \(([^\n]+)\) \{\n(.*?)^    \}', body, re.M | re.S))
    if not switches or body.count('switch (') != len(switches):
        raise ValueError(f'Unsupported switch structure in primary opcode {primary}')
    result = []
    for switch in switches:
        expression, cases = switch[1], switch[2]
        term_pattern = r'ExtractBits\(code, (\d+), (\d+)\) << (\d+)'
        terms = [tuple(map(int, match)) for match in re.findall(term_pattern, expression)]
        leftover = re.sub(term_pattern, '', expression)
        if not terms or re.sub(r'[()|\s]', '', leftover):
            raise ValueError(f'Unsupported equation: {expression}')
        for first, last, shift in terms:
            if not 0 <= first <= last < 32 or not 0 <= shift < 32:
                raise ValueError('Invalid source bit range')
        alternatives = {}
        for line in cases.splitlines():
            if not line.strip():
                continue
            match = re.fullmatch(r'      case (0b[01]+): PPC_DECODER_HIT\((\w+)\);', line)
            if not match:
                raise ValueError(f'Unsupported case syntax: {line}')
            key = int(match[1], 2)
            if key in alternatives:
                raise ValueError('Duplicate lookup case')
            alternatives[key] = match[2]
        result.append({'source_line': source[:start + switch.start()].count('\n') + 1,
                       'equation': expression, 'terms': terms,
                       'alternatives': {str(k): v for k, v in alternatives.items()}})
    return result


def classify(word, tables):
    primary = extract_bits(word, 0, 5)
    trace = []
    for index, fragment in enumerate(tables[primary]):
        value = 0
        for first, last, shift in fragment['terms']:
            value |= extract_bits(word, first, last) << shift
        hit = fragment['alternatives'].get(str(value))
        trace.append({'fragment_index': index, 'source_line': fragment['source_line'],
                      'equation': fragment['equation'], 'evaluated_key': value,
                      'checked_alternatives': len(fragment['alternatives']), 'hit': hit})
        if hit:
            return hit, trace
    return None, trace


def main():
    raw_input = INPUT.read_bytes()
    rows = json.loads(raw_input)
    if len(rows) != 38 or any(row['reported_mnemonic'] not in EXPECTED for row in rows):
        raise ValueError('Expected the current five-mnemonic, 38-word evidence set')
    # Resolve the moving source reference once; retain the immutable URL and hash.
    revision_url = 'https://api.github.com/repos/xenia-project/xenia/commits/master'
    commit = json.loads(fetch(revision_url))['sha']
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Invalid official source commit')
    source_url = f'https://raw.githubusercontent.com/xenia-project/xenia/{commit}/{SOURCE_PATH}'
    source_bytes = fetch(source_url)
    source = source_bytes.decode('utf8')
    extract_equation = 'return (v >> (32 - 1 - b)) & ((1 << (b - a + 1)) - 1);'
    if extract_equation not in source:
        raise ValueError('Upstream ExtractBits equation changed')
    primaries = sorted({extract_bits(int(row['raw_word'], 16), 0, 5) for row in rows})
    if primaries != [4, 5, 31, 63]:
        raise ValueError(f'Unexpected primary opcode coverage: {primaries}')
    tables = {primary: fragments(source, primary) for primary in primaries}
    classifications = []
    for row in rows:
        word = int(row['raw_word'], 16)
        if not 0 <= word <= 0xffffffff:
            raise ValueError('Not a 32-bit instruction word')
        name, trace = classify(word, tables)
        classifications.append({**row, 'primary_opcode': extract_bits(word, 0, 5),
            'xenia_classification': name, 'expected_classification': EXPECTED[row['reported_mnemonic']],
            'matches_expected': name == EXPECTED[row['reported_mnemonic']],
            'ordered_lookup_trace': trace})
    report = {'scope': 'Xenia source-decoder classification only; not hardware execution or instruction semantic proof',
        'source_repository': REPOSITORY, 'source_commit': commit, 'source_url': source_url,
        'source_revision_api': revision_url, 'source_sha256': hashlib.sha256(source_bytes).hexdigest(),
        'input_path': str(INPUT.relative_to(ROOT)), 'input_sha256': hashlib.sha256(raw_input).hexdigest(),
        'extract_bits_equation_cpp': extract_equation,
        'extract_bits_equation_python': '(word >> (32 - 1 - last)) & ((1 << (last - first + 1)) - 1)',
        'primary_opcode_equation': 'ExtractBits(code, 0, 5)',
        'method': 'Evaluate every earlier switch and all its case alternatives in original source order; first hit returns',
        'lookup_fragments': tables, 'classifications': classifications,
        'summary': {'words': len(rows), 'matched_expected': sum(row['matches_expected'] for row in classifications),
            'xenia_classifications': dict(Counter(row['xenia_classification'] for row in classifications))}}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['summary']))
    print(f'Evidence: {OUTPUT}')
    if report['summary']['matched_expected'] != len(rows):
        raise ValueError('Source-decoder classifications differ from expected; inspect output')


if __name__ == '__main__':
    main()
