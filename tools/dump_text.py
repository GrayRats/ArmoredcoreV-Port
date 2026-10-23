#!/usr/bin/env python3
"""Dump a guest address range via read-only Ghidra RPC read-bytes into a raw file.

usage: dump_text.py START END OUT   (hex addresses; START/END aligned to 4)
"""
import json, os, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
start, end, out = int(sys.argv[1], 16), int(sys.argv[2], 16), sys.argv[3]
env = dict(os.environ, GHIDRA_RPC_NO_AUTOSTART="1")
with open(out, "wb") as f:
    a = start
    while a < end:
        n = min(0x10000, end - a)
        p = subprocess.run([os.path.join(root, "scripts/ghidra-rpc.sh"), "read-bytes",
                            "/default.xex", hex(a), hex(n)], capture_output=True, text=True, env=env)
        r = json.loads(p.stdout)
        if not r.get("ok"):
            sys.exit(f"read failed at {a:#x}: {r}")
        f.write(bytes.fromhex(r["result"]["hex"]))
        a += n
