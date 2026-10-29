#!/usr/bin/env python3
"""Compile actual bounded recorder, exercise gate/capacity/shutdown and tail metrics."""
import json
import math
import subprocess
import tempfile
from pathlib import Path
from analyze_swap_trace import summarize

HERE = Path(__file__).resolve().parent
SOURCE = r'''
#include "swap_trace.h"
#include <cassert>
#include <filesystem>
int main(int, char** argv) {
 using namespace acv::swap_trace;
 std::string base=argv[1];
 setenv("ACV_SWAP_TRACE",(base+"/gate.jsonl").c_str(),1);
 assert(begin()==nullptr);
 assert(!std::filesystem::exists(base+"/gate.jsonl"));
 // Options path has explicit stable lifetime for all recorder writes.
 std::string full=base+"/full.jsonl";
 {
  Recorder<> r({full.c_str(),120,1.9});
  r.add(0,0,0,119.9);assert(!std::filesystem::exists(full));
  unsigned long long p=1000000000;
  r.add(p,p,p+2000000,120);
  for(int i=1;i<=100;++i) {
   p+=10000000;
   r.add(p-1000000,p,p+2000000,120+i*.01);
   assert(!std::filesystem::exists(full));
  }
  p+=1000000000;
  r.add(p-1000000,p,p+2000000,122);
  assert(r.stopped());
  r.add(p+10000000,p+10000000,p+10000000,123);
 }
 std::string cap=base+"/capacity.jsonl";
 {
  Recorder<3> r({cap.c_str(),120,60});
  for(int i=0;i<10;++i)r.add(i*10000000,i*10000000,i*10000000,120+i*.01);
  assert(r.stopped());
 }
 std::string partial=base+"/partial.jsonl";
 {Recorder<> r({partial.c_str(),120,60});r.add(0,0,0,120);}
 std::string invalid=base+"/invalid.jsonl";
 {Recorder<> r({invalid.c_str(),89,60});r.add(0,0,0,120);}
 assert(!std::filesystem::exists(invalid));
 std::string bad=base+"/bad-clock.jsonl";
 {Recorder<> r({bad.c_str(),120,60});r.add(5,6,7,120);r.add(6,7,8,121);}
 // Exclusive output must preserve previous bytes and report diagnostic failure.
 {Recorder<> r({full.c_str(),120,1});r.add(0,0,0,120);r.add(1000000000,1000000000,1000000000,121);}
}
'''
with tempfile.TemporaryDirectory(prefix="acv-swap-trace-host-") as directory:
    root = Path(directory)
    source = root / "host.cpp"
    source.write_text(SOURCE)
    subprocess.run(["clang++", "-std=c++23", "-O1", "-g", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
                    "-I", str(HERE), str(source), "-o", str(root / "host")], check=True)
    run = subprocess.run([str(root / "host"), str(root)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert run.stderr == "[acv-swap-trace] Cannot write exclusive diagnostic file\n", run.stderr
    full = summarize(root / "full.jsonl")
    assert full["complete"] and full["intervals"] == 101 and full["swaps"] == 102, full
    assert math.isclose(full["swap_rate"], 50.5) and full["p99_ms"] == 10, full
    assert math.isclose(full["one_percent_low"], 1000 / 505), full
    assert full["slowest_one_percent_count"] == 2 and full["max_ms"] == 1000
    for row in full["worst"]:
        assert math.isclose(row["interval_ms"], sum(row[k] for k in
                            ["previous_post_pacing_cpu_ms", "between_swap_calls_ms", "limiter_cpu_ms"]))
    cap = summarize(root / "capacity.jsonl")
    assert not cap["complete"] and cap["stop_reason"] == "capacity" and cap["swaps"] == 3
    partial = json.loads((root / "partial.jsonl").read_text().splitlines()[0])
    assert not partial["complete"] and partial["stop_reason"] == "shutdown"
    bad = json.loads((root / "bad-clock.jsonl").read_text().splitlines()[0])
    assert not bad["complete"] and bad["stop_reason"] == "invalid_clock_order"
    lines = (root / "full.jsonl").read_text().splitlines()
    (root / "truncated.jsonl").write_text("\n".join(lines[:-1]) + "\n")
    try:
        summarize(root / "truncated.jsonl")
    except ValueError:
        pass
    else:
        raise AssertionError("Truncated trace accepted")
    sdk = HERE.parent / "rexglue-sdk/src/graphics/acv_swap_trace.h"
    assert sdk.read_bytes() == (HERE / "swap_trace.h").read_bytes()
    print("PASS actual C++ ASan/UBSan recorder: real120s gate, synthetic full/capacity/shutdown/clock/error paths, exclusive output, exact tail metrics and interval decomposition; host-only")
