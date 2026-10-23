#!/usr/bin/env python3
"""Disassemble a range of a raw PowerPC text dump (guest base 0x822B0000) with llvm-mc.

One word per llvm-mc input line, so every output line maps to exactly one address.
Words that llvm-mc cannot decode (e.g. 0x00000000 padding) are printed as '.long 0x........'
instead of being dropped, so addresses never drift.
usage: ppc_dis.py DUMP START END   (hex guest addresses)"""
import re, subprocess, sys

dump, s, e = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
d = open(dump, 'rb').read()[s - 0x822B0000:e - 0x822B0000]
d = d[:len(d) // 4 * 4]
words = [d[i:i + 4] for i in range(0, len(d), 4)]
lines = '\n'.join(' '.join('0x%02x' % b for b in w) for w in words) + '\n'
proc = subprocess.run(['llvm-mc', '--disassemble', '--triple=powerpc64-unknown-unknown', '-mcpu=pwr7'],
                      input=lines, capture_output=True, text=True)
bad = {int(m.group(1)) for m in re.finditer(r'<stdin>:(\d+):\d+: warning: invalid instruction', proc.stderr)}
ok = [l.strip() for l in proc.stdout.splitlines() if l.strip() and not l.strip().startswith('.text')]
it = iter(ok)
for n, w in enumerate(words, start=1):
    a = s + 4 * (n - 1)
    if n in bad:
        text = '.long 0x%s' % w.hex()
    else:
        text = next(it, '<missing>')
        m = re.search(r'\b(bl?|bdnz|bt|bf|beq|bne|blt|bgt|ble|bge)\s+(?:\d+, )?\.([+-]\d+)', text)
        if m:
            text += f'    ; -> {(a + int(m.group(2))) & 0xffffffff:08x}'
    print(f'{a:08x}  {text}')
