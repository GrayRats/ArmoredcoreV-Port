#!/usr/bin/env python3
"""Exercise the private queue's real implementation, not GPU/scene semantics."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
header = root / "tools/native-rexglue/gpu-plugin/deferred_resolve_test.h"
harness = r'''#include <array>
#include <cassert>
#include <cstring>
#include <mutex>
#include <string>
using VkBuffer=unsigned long long;
#include "@HEADER@"
namespace rex::thread { bool held=false; }
int main(int argc,char** argv) {
 assert(argc==2);setenv("ACV_TRACE_BAKE_CHAIN",argv[1],1);
 setenv("ACV_DEFER_RESOLVE_START","0x1000",1);
 setenv("ACV_DEFER_RESOLVE_LENGTH","0x1000",1);
 assert(AcvDeferredEligible(0x1000,16));
 assert(!AcvDeferredEligible(0xfff,16));
 assert(!AcvDeferredEligible(0x1fff,2));
 rex::graphics::CommandProcessor cp;
 auto& s=AcvDeferred();std::array<uint8_t,16> cpu{},gpu{};gpu.fill(7);
 int waits=0,publishes=0;
 auto job=[&](bool ready=true) {return AcvDeferredJob{1,0x1000,16,4,cpu.data(),{},
  [&,ready]{assert(!rex::thread::held);++waits;return ready;},
  [&]{assert(rex::thread::held);++publishes;cpu=gpu;return true;}};};
 assert(AcvAddDeferred(&cp,job()));assert(cpu[0]==0 && waits==0 && publishes==0);
 assert(AcvResolveReadbackForCpu(0x1000,16));
 assert(cpu==gpu && waits==1 && publishes==1 && s.jobs.empty());
 // CPU writes invalidate the lease; no stale GPU publication is permitted.
 assert(AcvAddDeferred(&cp,job()));cpu[3]=9;
 assert(!AcvFlushDeferred(&cp,"test_cpu_write"));
 assert(cpu[3]==9 && publishes==1 && s.jobs.empty());
 s.failed=false;
 // A failed GPU completion never enters the publication callback.
 assert(AcvAddDeferred(&cp,job(false)));
 assert(!AcvRetireDeferredSlot(&cp,1));assert(publishes==1);
 s.failed=false;cpu.fill(0);
 // Validate all overlapping baselines before publishing either job.
 assert(AcvAddDeferred(&cp,job()));assert(AcvAddDeferred(&cp,job()));
 assert(AcvRetireDeferredSlot(&cp,1));assert(publishes==3 && cpu==gpu);
 assert(s.jobs.empty() && !rex::thread::held);
 AcvResetDeferred(&cp);assert(s.processor.load()==nullptr);
 std::puts("PASS: actual deferred queue preserves wait/host-lock order, rejects CPU mutation and failed completion, retires overlapping slots safely");
}
'''
with tempfile.TemporaryDirectory(prefix="acv-deferred-") as temporary:
    tmp=Path(temporary)
    (tmp/"rex/graphics").mkdir(parents=True)
    (tmp/"rex/graphics/command_processor.h").write_text("#pragma once\nnamespace rex::graphics {struct CommandProcessor {template<class F> void CallInThread(F f){f();}};}\n")
    (tmp/"rex/thread.h").write_text("#pragma once\n#include <cassert>\nnamespace rex::thread {extern bool held; struct global_critical_region {struct Lock {Lock(){assert(!held);held=true;} ~Lock(){held=false;}}; Lock Acquire(){return {};}};}\n")
    source=tmp/"probe.cpp"
    source.write_text(harness.replace("@HEADER@",str(header)))
    subprocess.run(["clang++","-std=c++23","-fsanitize=address,undefined","-fno-omit-frame-pointer","-g","-I",str(tmp),str(source),"-pthread","-o",str(tmp/"probe")],check=True)
    # Expected refusal diagnostics are checked/captured, not runtime failures.
    result=subprocess.run([str(tmp/"probe"),str(tmp/"trace")],capture_output=True,text=True,check=True)
    assert result.stderr.count("refusing publication")==2, result.stderr
    print(result.stdout,end="")
