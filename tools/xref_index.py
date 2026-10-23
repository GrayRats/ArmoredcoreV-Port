#!/usr/bin/env python3
"""Read-only cross-reference index over a raw PowerPC .text dump (guest base 0x822B0000).

Inputs are produced by tools/dump_text.py (.text) and a .pdata dump (0x82216A00..0x822A5CD8).
Builds, without touching the Ghidra database:
  - function bounds from .pdata (length in words, bits 8..29 of the second word)
  - direct b/bl edges (caller instruction -> target), with owners on both sides
  - 32-bit address constants built by lis + addi/ori pairs (within 8 instructions)
  - immediate-displacement stores, grouped by displacement

usage:
  xref_index.py TEXT PDATA OUT.json [--callers HEX] [--disp HEX]

--callers HEX   print direct callers of a guest address
--disp HEX      print stores with that displacement and their owners
Output is JSON at OUT.json; the CLI prints a short summary and the requested query.
"""
import bisect, json, struct, sys

TEXT_BASE = 0x822B0000
STORE_OPS = {36: 'stw', 37: 'stwu', 38: 'stb', 44: 'sth', 52: 'stfs', 54: 'stfd', 62: 'std'}


def load_functions(pdata_path):
    pd = open(pdata_path, 'rb').read()
    funcs = []
    for i in range(0, len(pd) - 7, 8):
        begin, word = struct.unpack('>II', pd[i:i + 8])
        length = ((word >> 8) & 0x3FFFFF) * 4
        if length:
            funcs.append((begin, length))
    funcs.sort()
    return funcs


class Owners:
    def __init__(self, funcs):
        self.starts = [b for b, _ in funcs]
        self.funcs = funcs

    def __call__(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0:
            b, n = self.funcs[i]
            if addr < b + n:
                return b
        return None


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        sys.exit(__doc__)
    text_path, pdata_path, out_path = args[:3]
    callers_q = int(args[args.index('--callers') + 1], 16) if '--callers' in args else None
    disp_q = int(args[args.index('--disp') + 1], 16) if '--disp' in args else None

    raw = open(text_path, 'rb').read()
    raw = raw[:len(raw) // 4 * 4]
    words = struct.unpack('>%dI' % (len(raw) // 4), raw)
    owner = Owners(load_functions(pdata_path))

    calls, addr_refs, stores = [], [], {}
    pending_hi = {}  # register -> (index, hi)
    for i, x in enumerate(words):
        a = TEXT_BASE + 4 * i
        op = x >> 26
        if op == 18 and not (x & 2):  # b / bl, absolute bit clear
            off = x & 0x03FFFFFC
            if off & 0x02000000:
                off -= 0x04000000
            t = (a + off) & 0xFFFFFFFF
            calls.append({'src': a, 'dst': t, 'link': bool(x & 1),
                          'src_fn': owner(a), 'dst_fn': owner(t)})
        if op in STORE_OPS:
            disp = x & 0xFFFF
            stores.setdefault(disp, []).append({'at': a, 'op': STORE_OPS[op],
                                                'reg': (x >> 21) & 31, 'base': (x >> 16) & 31,
                                                'fn': owner(a)})
        if op == 15:  # lis rD, hi (rA == 0 means literal; rA != 0 is addis, skipped)
            if ((x >> 16) & 31) == 0:
                pending_hi[(x >> 21) & 31] = (i, x & 0xFFFF)
        elif op in (14, 24) and ((x >> 16) & 31) != 0 and ((x >> 16) & 31) in pending_hi:
            rd = (x >> 16) & 31
            hi_i, hi = pending_hi[rd]
            if i - hi_i <= 8:
                lo = x & 0xFFFF
                if op == 14:  # addi: signed low part
                    val = ((hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
                else:  # ori
                    val = ((hi << 16) | lo) & 0xFFFFFFFF
                addr_refs.append({'at': a, 'value': val, 'fn': owner(a)})

    index = {'functions': [{'begin': b, 'length': n} for b, n in owner.funcs],
             'calls': calls, 'addr_refs': addr_refs,
             'stores_by_disp': {f'{d:#x}': v for d, v in stores.items()}}
    with open(out_path, 'w') as f:
        json.dump(index, f)

    print(f'functions={len(owner.funcs)} calls={len(calls)} '
          f'addr_refs={len(addr_refs)} store_displacements={len(stores)}')
    if callers_q is not None:
        for c in calls:
            if c['dst'] == callers_q:
                print(f"call {c['src']:08x} -> {callers_q:08x}  (in {c['src_fn']:08x})" if c['src_fn'] is not None
                      else f"call {c['src']:08x} -> {callers_q:08x}  (no pdata owner)")
    if disp_q is not None:
        for s in stores.get(disp_q, []):
            print(f"{s['at']:08x} {s['op']} r{s['reg']}, {disp_q:#x}(r{s['base']})  in "
                  f"{s['fn']:08x}" if s['fn'] is not None else f"{s['at']:08x} {s['op']} (no owner)")


if __name__ == '__main__':
    main()
