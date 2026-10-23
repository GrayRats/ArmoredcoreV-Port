#!/usr/bin/env python3
"""Register-aware backward slice over a Xenia-translated SPIR-V disassembly (SSA names _N).

Resolves pointers to xe_var_registers[k] to the last store to register k before the load line,
follows SSA definitions, and stops at leaves: float/system constants (with their names and index),
texture samples, interpolants and unresolved pointers. Control flow is approximated (the last
textual store wins), so the result is a candidate chain, not an exact dataflow proof.

Usage: spirv_slice.py DISASM.txt SEED_NAME DEPTH   (SEED_NAME like 1964 or _1964)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

DEF = re.compile(r'^\s*(?:\[\[[^\]]*\]\]\s*)?(?:const\s+)?([A-Za-z0-9_<>]+\*?)\s+(_\d+)\s*=\s*(.*?);\s*$')
STORE = re.compile(r'^\s*\*\s*(_\d+)\s*=\s*(_\d+)\s*;\s*$')
REF = re.compile(r'\b_(\d+)\b')
REG_PTR = re.compile(r'&xe_var_registers\[(\d+)\]')
CONST_PTR = re.compile(r'&xe_uniform_float_constants\.float_constants\[(\d+)\]')
SYS_PTR = re.compile(r'&xe_uniform_system_constants\.(\w+)')


class Slicer:
    def __init__(self, path: Path):
        self.lines = path.read_text().split('\n')
        self.defs: dict[str, tuple[int, str, str]] = {}
        self.stores: list[tuple[int, str, str]] = []  # (line, pointer, value)
        for i, line in enumerate(self.lines, start=1):
            m = DEF.match(line)
            if m and m.group(2) not in self.defs:
                self.defs[m.group(2)] = (i, m.group(1), m.group(3))
            s = STORE.match(line)
            if s:
                self.stores.append((i, s.group(1), s.group(2)))
        self.leaves: list[str] = []

    def pointer_target(self, ptr: str):
        if ptr not in self.defs:
            return ('unknown', None)
        _, _, expr = self.defs[ptr]
        m = REG_PTR.search(expr)
        if m:
            return ('register', int(m.group(1)))
        m = CONST_PTR.search(expr)
        if m:
            return ('const', int(m.group(1)))
        m = SYS_PTR.search(expr)
        if m:
            return ('sys', m.group(1))
        return ('other', expr[:60])

    def last_store_to_register(self, reg: int, before_line: int):
        best = None
        for line, ptr, value in self.stores:
            if line >= before_line:
                break
            kind, idx = self.pointer_target(ptr)
            if kind == 'register' and idx == reg:
                best = (line, value)
        return best

    def walk(self, name: str, depth: int, out: list[str], indent: int = 0, seen: set | None = None):
        seen = set() if seen is None else seen
        if name in seen:
            return
        seen.add(name)
        if name not in self.defs:
            out.append('  ' * indent + f'{name}: <no def>')
            return
        line, typ, expr = self.defs[name]
        out.append('  ' * indent + f'{name} [L{line}] {typ} = {expr[:120]}')
        if 'Sample' in expr or 'ImageSample' in expr or 'Texture' in expr:
            self.leaves.append(f'texture sample {name} [L{line}]')
            return
        if expr.startswith('*'):
            ptr = expr[1:].strip()
            kind, idx = self.pointer_target(ptr)
            if kind == 'const':
                self.leaves.append(f'float_constant[{idx}] (L{line})')
                return
            if kind == 'sys':
                self.leaves.append(f'system_constant.{idx} (L{line})')
                return
            if kind == 'register':
                st = self.last_store_to_register(idx, line)
                if st is None:
                    self.leaves.append(f'register[{idx}] (no earlier store, L{line})')
                    return
                out.append('  ' * (indent + 1) + f'register[{idx}] <- store L{st[0]}')
                if depth > 0:
                    self.walk(st[1], depth - 1, out, indent + 2, seen)
                return
            self.leaves.append(f'load {ptr} ({kind}, L{line})')
            return
        if 'xe_var_fragment_data' in expr or 'Input' in expr or 'xe_var_interpolator' in expr:
            self.leaves.append(f'input/interpolant {name} [L{line}]')
            return
        if 'Phi(' in expr:
            self.leaves.append(f'phi {name} [L{line}] (control flow merge, not followed)')
            return
        if depth <= 0:
            return
        for ref in REF.findall(expr)[:4]:
            self.walk('_' + ref, depth - 1, out, indent + 1, seen)


def main() -> int:
    slicer = Slicer(Path(sys.argv[1]))
    seed = sys.argv[2] if sys.argv[2].startswith('_') else '_' + sys.argv[2]
    depth = int(sys.argv[3])
    out: list[str] = []
    slicer.walk(seed, depth, out)
    print('\n'.join(out))
    print('--- leaves ---')
    for leaf in sorted(set(slicer.leaves)):
        print(leaf)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
