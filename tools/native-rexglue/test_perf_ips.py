#!/usr/bin/env python3
"""Exercise the actual Linux perf ring and same-session ELF symbol resolution."""
import json, subprocess, tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="acv-perf-ip-") as directory:
    root=Path(directory)
    (root/"busy.cpp").write_text('extern "C" __attribute__((noinline)) void hot(){volatile unsigned long n=0;for(;;)n=n+1;} int main(){hot();}')
    subprocess.run(["c++","-g","-O1",str(root/"busy.cpp"),"-o",str(root/"busy")],check=True)
    child=subprocess.Popen([str(root/"busy")])
    try:
        subprocess.run(["python3",str(HERE/"perf_ips.py"),"--pid",str(child.pid),"--executable",str(root/"busy"),"--output",str(root/"out"),"--seconds","2","--after-start","0"],check=True)
        subprocess.run(["python3",str(HERE/"perf_symbolize.py"),str(root/"out")],check=True)
        data=json.loads((root/"out/summary.json").read_text())
        assert data["threads"][0]["samples"]>=10,data
        assert sum(t["lost"] for t in data["threads"])==0,data
        top=data["threads"][0]["top"][0]
        assert top["symbol"]=="hot" and top["thread_percent"]>=80,top
        print("PASS user CPU ring: exact hot symbol dominates, matching process maps, no lost samples")
    finally:
        child.terminate();child.wait()
