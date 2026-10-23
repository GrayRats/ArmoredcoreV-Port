#!/usr/bin/env python3
"""Read-only scan of a raw PowerPC text dump (base 0x822B0000) for stores to given displacements.

usage: scan_field_writes.py DUMP PDATA_DUMP DISP[,DISP...]   (hex displacements, e.g. 910,914)
Prints each matching store with the containing function from .pdata. Reports only
displacement-immediate stores (stw/std/stfs/stfd/sth/stb); register-indexed stores
(stwx etc.) and copies are NOT detected and need a separate pass.
"""
import struct, sys

TEXT_BASE, PD_BASE = 0x822B0000, 0x82216A00
dump, pdump, disps = sys.argv[1], sys.argv[2], {int(x, 16) for x in sys.argv[3].split(',')}
t = open(dump, 'rb').read()
t = t[:len(t) // 4 * 4]
words = struct.unpack('>%dI' % (len(t) // 4), t)
pd = open(pdump, 'rb').read()
funcs = sorted((b, ((w >> 8) & 0x3FFFFF) * 4) for b, w in
               (struct.unpack('>II', pd[i:i + 8]) for i in range(0, len(pd) - 7, 8)))

def owner(a):
    lo, hi = 0, len(funcs) - 1
    while lo <= hi:
        m = (lo + hi) // 2
        b, n = funcs[m]
        if a < b: hi = m - 1
        elif a >= b + n: lo = m + 1
        else: return b
    return None

STORES = {36: 'stw', 37: 'stwu', 38: 'stb', 44: 'sth', 52: 'stfs', 54: 'stfd', 62: 'std'}
for i, x in enumerate(words):
    op = x >> 26
    if op in STORES and (x & 0xFFFF) in disps:
        a = TEXT_BASE + 4 * i
        f = owner(a)
        print(f"{a:08x} {STORES[op]:4s} r{(x >> 21) & 31}, {x & 0xFFFF:#x}(r{(x >> 16) & 31})  in {f:08x}" if f else
              f"{a:08x} {STORES[op]:4s} r{(x >> 21) & 31}, {x & 0xFFFF:#x}(r{(x >> 16) & 31})  (no pdata)")
