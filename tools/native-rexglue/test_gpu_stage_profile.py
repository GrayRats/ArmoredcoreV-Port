#!/usr/bin/env python3
"""Actual C++ stage timer gate and deterministic host-only JSON aggregation."""
import json,subprocess,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="acv-gpu-stage-host-") as directory:
    root=Path(directory)
    source=r'''
#include "gpu_stage_profile.h"
#include <cassert>
#include <filesystem>
int main(int, char** argv){
 setenv("ACV_GPU_STAGE_PROFILE",argv[1],1);
 const auto age=acv::gpu_profile::configuration().age();
 assert(age>=0 && age<90);
 for (size_t i=0; i<size_t(acv::gpu_profile::Stage::kCount); ++i) {
   acv::gpu_profile::Scope scope(static_cast<acv::gpu_profile::Stage>(i));
 }
 assert(!std::filesystem::exists(argv[1]));
 // Synthetic host aggregation timestamps; never a gameplay capture.
 acv::gpu_profile::State data;
 data.add(acv::gpu_profile::Stage::kFence,1000000,90);
 data.add(acv::gpu_profile::Stage::kFence,2000000,92);
 for (size_t i=size_t(acv::gpu_profile::Stage::kSlotAllocate);
      i<size_t(acv::gpu_profile::Stage::kCount); ++i)
   data.add(static_cast<acv::gpu_profile::Stage>(i),500000,94);
 data.add(acv::gpu_profile::Stage::kDraw,4000000,95.1);
}
'''
    (root/"test.cpp").write_text(source)
    subprocess.run(["c++","-std=c++23","-O2","-Wall","-Wextra","-I",str(HERE),str(root/"test.cpp"),"-o",str(root/"test")],check=True)
    subprocess.run([str(root/"test"),str(root/"host-only.jsonl")],check=True)
    rows=[json.loads(s) for s in (root/"host-only.jsonl").read_text().splitlines()]
    assert len(rows)==1,rows
    assert rows[0]["stages"]["submission_fence"]=={"calls":2,"total_ms":3,"max_ms":2},rows
    assert rows[0]["stages"]["draw"]=={"calls":1,"total_ms":4,"max_ms":4},rows
    for name in ["slot_allocate", "slot_reuse_pending", "slot_reuse_ready", "wait_full", "wait_previous_missing", "wait_previous_incomplete", "previous_ready", "invalidate", "publish_copy", "wait_previous_generation", "fast_readback_submit", "general_resolve", "general_wait", "general_publish", "general_submit", "general_fallback", "general_cpu_wait", "general_pool_wait", "general_admission", "general_fitting_slot", "general_pool_drain"]:
        assert rows[0]["stages"][name]=={"calls":1,"total_ms":0.5,"max_ms":0.5},rows
    assert rows[0]["scope"]=="inclusive CPU wall time"
    print("PASS actual C++ Scope produces no file before90s; host-only aggregation counts/durations/JSON verified")
