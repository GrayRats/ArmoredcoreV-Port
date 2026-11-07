#!/usr/bin/env python3
"""Read-only x86-64 perf samples of a verified stack-neutral memcpy loop.

Only the first user-stack word (return address), RCX, RDX, RSI, RDI and RSP are requested.
Interpretation is restricted to the separately disassembled libc backward loop.
No ptrace, process signals, injected code or runtime-setting writes.
"""
import argparse
import collections
import ctypes
import fcntl
import hashlib
import json
import mmap
import os
import platform
import struct
import time
from pathlib import Path
from perf_gate import process_identity, wait_for_process_age

PAGE = os.sysconf("SC_PAGE_SIZE")
SAMPLE_TYPE = 3 | (1 << 12) | (1 << 13)
EXPECTED_LIBC_SHA256 = "071e34179e645b25f8a8c2e7108894dc665caa9f5f8c939cd579a75b6638bc3f"
REGS_MASK = sum(1 << bit for bit in (2, 3, 4, 5, 7))  # CX, DX, SI, DI, SP


class CopyRing:
    def __init__(self, tid):
        attr = ctypes.create_string_buffer(128)
        struct.pack_into("IIQQQQQ", attr, 0, 1, 128, 0, 20000000, SAMPLE_TYPE, 0,
                         1 | (1 << 5) | (1 << 6))
        struct.pack_into("I", attr, 48, 1)
        struct.pack_into("Q", attr, 80, REGS_MASK)
        struct.pack_into("I", attr, 88, 8)
        libc = ctypes.CDLL(None, use_errno=True)
        self.fd = libc.syscall(298, ctypes.byref(attr), tid, -1, -1, 8)
        if self.fd < 0:
            raise OSError(ctypes.get_errno(), os.strerror(ctypes.get_errno()))
        try:
            self.mapping = mmap.mmap(self.fd, PAGE*9, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
            self.offset, self.size = struct.unpack_from("QQ", self.mapping, 1040)
            self.tail, self.lost, self.samples = 0, 0, 0
            fcntl.ioctl(self.fd, 0x2400, 0)
        except BaseException:
            os.close(self.fd)
            raise

    def read(self, consume):
        head = struct.unpack_from("Q", self.mapping, 1024)[0]
        if head - self.tail > self.size:
            raise RuntimeError("Perf ring overrun")
        while self.tail < head:
            position = self.tail % self.size
            def data(n):
                part = self.mapping[self.offset+position:self.offset+min(position+n, self.size)]
                return part + self.mapping[self.offset:self.offset+max(0, n-len(part))]
            typ, misc, size = struct.unpack("IHH", data(8))
            if size < 8 or size > self.size or self.tail+size > head:
                raise RuntimeError("Invalid perf record")
            record = data(size)
            if typ == 9:
                self.samples += 1
                consume(record)
            elif typ == 2:
                self.lost += struct.unpack_from("Q", record, 16)[0]
            self.tail += size
        struct.pack_into("Q", self.mapping, 1032, self.tail)

    def close(self, consume):
        try:
            fcntl.ioctl(self.fd, 0x2401, 0)
            self.read(consume)
        finally:
            self.mapping.close()
            os.close(self.fd)


def decode(record):
    ip, pid, tid = struct.unpack_from("QII", record, 8)
    abi = struct.unpack_from("Q", record, 24)[0]
    if abi == 0:
        return None
    if abi != 2:
        raise RuntimeError("Expected PERF_SAMPLE_REGS_ABI_64")
    cursor_destination, length, cursor_source, destination, sp = struct.unpack_from("QQQQQ", record, 32)
    stack_size = struct.unpack_from("Q", record, 72)[0]
    if stack_size == 0:
        return None
    if stack_size != 8 or len(record) < 96:
        raise RuntimeError("Unexpected user-stack sample layout")
    ret, dynamic_size = struct.unpack_from("QQ", record, 80)
    if dynamic_size < 8:
        return None
    return {"ip": ip, "pid": pid, "tid": tid, "rdx": length,
            "rdi": destination, "rsp": sp, "rcx": cursor_destination, "rsi": cursor_source, "return_ip": ret}


def capture(pid, executable, tids, seconds, output, after_start=90):
    if platform.machine() != "x86_64":
        raise RuntimeError("Only verified x86-64 perf ABI is supported")
    gate = wait_for_process_age(pid, executable, after_start)
    root = Path("/proc") / str(pid)
    output.mkdir(parents=True, exist_ok=False)
    maps = (root / "maps").read_text()
    (output / "maps.txt").write_text(maps)
    libc_maps = []
    libc = Path("/usr/lib/libc.so.6")
    libc_sha = hashlib.file_digest(libc.open("rb"), "sha256").hexdigest()
    if libc_sha != EXPECTED_LIBC_SHA256:
        raise RuntimeError("libc differs from verified instruction snapshot; revalidate loop before use")
    stat = libc.stat()
    for line in maps.splitlines():
        f = line.split(maxsplit=5)
        if len(f) == 6 and f[5] == str(libc):
            if int(f[4]) != stat.st_ino:
                raise RuntimeError("libc mapped inode differs from disk")
            lo, hi = (int(x, 16) for x in f[0].split("-"))
            libc_maps.append((lo, hi, int(f[2], 16)))
    if not libc_maps:
        raise RuntimeError("Expected libc mapping not found")
    samples = []
    counts = collections.Counter()
    skipped, truncated = 0, False
    def consume(record):
        nonlocal skipped, truncated
        row = decode(record)
        if row is None:
            skipped += 1
            return
        ip = row["ip"]
        offset = next((ip-lo+off for lo, hi, off in libc_maps if lo <= ip < hi), None)
        # Verified glibc loop preserves RDX(original length), RDI(original dest),
        # and RSP. This is not a generic unwinder or arbitrary-stack heuristic.
        if offset is None or not 0x1b1ba0 <= offset < 0x1b1bd3:
            return
        row["libc_offset"] = offset
        # RSI is decremented128bytes before RCX in this verified loop.
        adjustment = 128 if 0x1b1bb7 <= offset <= 0x1b1bca else 0
        row["source_original"] = row["rsi"] - row["rcx"] + row["rdi"] + adjustment
        src, dst, length = row["source_original"], row["rdi"], row["rdx"]
        row["nonoverlapping"] = src + length <= dst or dst + length <= src
        counts[row["tid"]] += 1
        if len(samples) < 20000:
            samples.append(row)
        else:
            truncated = True
    rings, failures = {}, {}
    names = {}
    started = time.time()
    deadline = time.monotonic() + seconds
    try:
        for tid in tids:
            task = root / "task" / str(tid)
            names[tid] = (task / "comm").read_text().strip()
            try:
                rings[tid] = CopyRing(tid)
            except OSError as error:
                failures[tid] = str(error)
        while time.monotonic() < deadline:
            if process_identity(pid) != gate["process_start_ticks"]:
                raise RuntimeError("Observed PID identity changed")
            for ring in rings.values():
                ring.read(consume)
            time.sleep(.05)
    finally:
        for ring in rings.values():
            ring.close(consume)
    lost = sum(r.lost for r in rings.values())
    result = {"pid": pid, "gate": gate, "started_unix": started, "finished_unix": time.time(),
              "scope": "50Hz user CPU samples of verified stack-neutral libc backward-copy loop; sampled return IPs, not call counts or elapsed memcpy latency",
              "libc_path": str(libc), "libc_sha256": libc_sha,
              "target_offsets": ["0x1b1ba0", "0x1b1bd3"], "names": names,
              "total_user_samples": sum(r.samples for r in rings.values()), "lost": lost,
              "unavailable": failures, "stack_unavailable_samples": skipped,
              "truncated": truncated, "hits": samples}
    (output / "samples.json").write_text(json.dumps(result, indent=2))
    print(json.dumps({"evidence": str(output), "total_user_samples": result["total_user_samples"],
                      "copy_hits": len(samples), "lost": lost, "unavailable": failures,
                      "stack_unavailable_samples": skipped, "truncated": truncated}))
    return 0 if samples and not lost and not failures and not truncated else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--tid", type=int, action="append", required=True)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 120:
        parser.error("--seconds must be1..120")
    raise SystemExit(capture(args.pid, args.executable, args.tid, args.seconds, args.output))
