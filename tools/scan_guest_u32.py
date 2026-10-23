#!/usr/bin/env python3
"""Read-only scan of Xenia guest memory for a big-endian 32-bit value.

Guest address G maps to host address 0x100000000 + G for the 0x80000000-range
image, per docs/ACV_TIMING.md (guest 0x8202411C -> host 0x18202411C). The
process must be readable via /proc/<pid>/mem (ptrace_scope 0 or same-process).

usage: scan_guest_u32.py PID START_HEX LENGTH_HEX NEEDLE_HEX

Prints every guest address in [START, START+LENGTH) whose 4 bytes equal NEEDLE.
Unreadable pages are skipped and counted.
"""
import struct
import sys

HOST_BASE = 0x100000000


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        return 2
    pid = int(sys.argv[1])
    start = int(sys.argv[2], 16)
    length = int(sys.argv[3], 16)
    needle = int(sys.argv[4], 16)
    needle_bytes = struct.pack(">I", needle)
    chunk = 1 << 20
    skipped = 0
    hits = []
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as mem:
        offset = start
        end = start + length
        while offset < end:
            size = min(chunk, end - offset)
            try:
                mem.seek(HOST_BASE + offset)
                data = mem.read(size)
            except (OSError, ValueError):
                skipped += size
                offset += size
                continue
            if len(data) != size:
                skipped += size - len(data)
            pos = data.find(needle_bytes)
            while pos != -1:
                if pos % 4 == 0:
                    hits.append(offset + pos)
                pos = data.find(needle_bytes, pos + 1)
            offset += size
    for addr in hits:
        print(f"{addr:08X}")
    print(f"hits={len(hits)} skipped_bytes={skipped}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
