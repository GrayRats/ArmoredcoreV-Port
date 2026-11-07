#!/usr/bin/env python3
"""Verify actual perf ABI/user-stack caller attribution against a host memcpy fixture."""
import json
import struct
import subprocess
import tempfile
from pathlib import Path
from perf_copy_sample import capture, decode

SOURCE = r'''
#include <cstdlib>
#include <cstring>
extern "C" __attribute__((noinline)) void copy_site(void* d, const void* s, size_t n) {
 std::memcpy(d,s,n);
 asm volatile("" : : "r"(d) : "memory");
}
int main() {
 const size_t n=4*1024*1024;
 void* p=nullptr;if(posix_memalign(&p,4096,2*n))return 1;
 std::memset(p,37,2*n);
 while(true)copy_site(static_cast<char*>(p)+n,p,n);
}
'''
with tempfile.TemporaryDirectory(prefix="acv-copy-perf-host-") as directory:
    root=Path(directory);(root/"host.cpp").write_text(SOURCE)
    subprocess.run(["clang++","-O2","-g","-fno-builtin-memcpy","-fno-omit-frame-pointer",
                    "-no-pie",str(root/"host.cpp"),"-o",str(root/"host")],check=True)
    child=subprocess.Popen([str(root/"host")])
    try:
        assert capture(child.pid,root/"host",[child.pid],3,root/"samples",after_start=0)==0
        data=json.loads((root/"samples/samples.json").read_text())
        assert not data["lost"] and not data["unavailable"] and not data["truncated"]
        assert len(data["hits"])>=5,data
        for h in data["hits"]:
            assert h["rdx"]==4*1024*1024 and h["rdi"]%4096==0
            assert h["source_original"]==h["rdi"]-4*1024*1024 and h["nonoverlapping"], h
        addresses=sorted(set(h["return_ip"]-1 for h in data["hits"]))
        result=subprocess.run(["eu-addr2line","-f","-e",str(root/"host"),
                               *[hex(v) for v in addresses]],capture_output=True,text=True,check=True)
        assert "copy_site" in result.stdout,result.stdout
        assert all(l=="copy_site" for l in result.stdout.splitlines()[::2]),result.stdout
        assert child.poll() is None
        print("PASS real perf regs/user-stack ABI: known memcpy caller, exact4MiB size/aligned destination, no losses; no process injection/signals during sampling")
    finally:
        child.terminate();child.wait()
