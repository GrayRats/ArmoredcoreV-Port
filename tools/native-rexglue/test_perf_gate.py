#!/usr/bin/env python3
"""Actual process-age gating: neither profiler may create output before delay."""
import json,subprocess,tempfile,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="acv-perf-gate-") as directory:
    root=Path(directory)
    (root/"busy.cpp").write_text("int main(){volatile unsigned long n=0;for(;;)n=n+1;}")
    subprocess.run(["c++","-O1",str(root/"busy.cpp"),"-o",str(root/"busy")],check=True)
    child=subprocess.Popen([str(root/"busy")]);jobs=[]
    try:
        for name in ["perf_ips.py","perf_sample.py"]:
            output=root/name
            jobs.append((output,subprocess.Popen(["python3",str(HERE/name),"--pid",str(child.pid),
                "--executable",str(root/"busy"),"--output",str(output),"--after-start","2","--seconds","1"],
                stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)))
        time.sleep(.75)
        assert all(not output.exists() for output,job in jobs),"Diagnostics created before gate"
        for output,job in jobs:
            text=job.communicate(timeout=8)[0]
            assert job.returncode==0,text
            gate=json.loads((output/"gate.json").read_text())
            assert gate["sampling_started_age_s"]>=2,gate
            if (output/"samples.jsonl").exists():
                rows=[json.loads(s) for s in (output/"samples.jsonl").read_text().splitlines()]
                assert rows and all(r["game_age_s"]>=2 for r in rows),rows
        print("PASS both actual profilers: no output before gate; recorded process age meets threshold")
    finally:
        for output,job in jobs:
            if job.poll() is None:job.terminate();job.wait()
        child.terminate();child.wait()
