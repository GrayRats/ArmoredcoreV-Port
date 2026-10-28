#!/usr/bin/env python3
"""Exercise the actual optional hook with an ABI stub and wrapped host sleep."""
import json, os, subprocess, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name("gpu_poll_backoff.cpp")
HEADER = r"""
#pragma once
#include <cstdint>
union Reg { uint64_t u64; uint32_t u32; };
struct PPCContext { uint64_t lr; Reg r3; uint64_t other[48]; };
#define REX_EXTERN(name) extern "C" void name(PPCContext& ctx, uint8_t* base)
"""
TEST = r"""
#include "acv_linux_probe_pch.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <time.h>
REX_EXTERN(sub_82BAE308);
static unsigned calls=0, sleeps=0;
static bool pending=true, throws=false, benchmark=false;
static uint8_t* expected_base=nullptr;
extern "C" int __real_nanosleep(const timespec*,timespec*);
extern "C" int __wrap_nanosleep(const timespec* req,timespec* rem) {
  ++sleeps;
  assert(req->tv_sec==0 && req->tv_nsec==50000);
  return benchmark ? __real_nanosleep(req,rem) : 0;
}
REX_EXTERN(__imp__sub_82BAE308) {
  ++calls; assert(base==expected_base);
  if(throws) throw std::runtime_error("original");
  for(unsigned i=0;i<48;++i)ctx.other[i]=i*9187+calls;
  ctx.r3.u64=pending?1:0;
  ctx.lr=0x82BAF2B0; // Model original restore of return LR.
}
int main(int argc,char**) {
  const bool enabled=std::getenv("ACV_GPU_POLL_BACKOFF") &&
    std::strcmp(std::getenv("ACV_GPU_POLL_BACKOFF"),"1")==0;
  uint8_t memory=0; expected_base=&memory; PPCContext ctx{};
  for(unsigned i=0;i<32;++i) {
    ctx.lr=0x82BAF2B0; sub_82BAE308(ctx,&memory);
    assert(ctx.r3.u64==1 && ctx.lr==0x82BAF2B0);
    for(unsigned k=0;k<48;++k)assert(ctx.other[k]==k*9187+calls);
  }
  assert(calls==32 && sleeps==(enabled?32:0));
  // Completed/cancelled helper result and unrelated call sites never sleep.
  pending=false; ctx.lr=0x82BAF2B0; sub_82BAE308(ctx,&memory);
  assert(ctx.r3.u64==0);
  pending=true; ctx.lr=0x82BAE000; sub_82BAE308(ctx,&memory);
  assert(sleeps==(enabled?32:0));
  // Propagate original exceptions without a scheduling side effect.
  throws=true;ctx.lr=0x82BAF2B0;
  bool caught=false; try {sub_82BAE308(ctx,&memory);}catch(const std::runtime_error&){caught=true;}
  assert(caught && sleeps==(enabled?32:0)); throws=false;
  if(argc>1) {
    benchmark=true; calls=0;sleeps=0;
    const auto start=std::chrono::steady_clock::now();
    const auto cpu=std::clock();
    do {
      ctx.lr=0x82BAF2B0;sub_82BAE308(ctx,&memory);
    }while(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(250));
    const double cpu_ms=1000.0*(std::clock()-cpu)/CLOCKS_PER_SEC;
    std::cout<<"{\"enabled\":"<<enabled<<",\"calls\":"<<calls
             <<",\"cpu_ms\":"<<cpu_ms<<",\"sleeps\":"<<sleeps<<"}\n";
  }
}
"""
def main():
    with tempfile.TemporaryDirectory(prefix="acv-poll-hook-") as temp:
        root=Path(temp)
        (root/"acv_linux_probe_pch.h").write_text(HEADER)
        (root/"test.cpp").write_text(TEST)
        exe=root/"test"
        subprocess.run(["clang++","-std=c++23","-O2","-g","-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer","-pthread","-I",str(root),
                        str(SOURCE),str(root/"test.cpp"),"-Wl,--wrap=nanosleep","-o",str(exe)],check=True)
        results=[]
        for value in ("0","1","10",""):
            env=os.environ.copy();env["ACV_GPU_POLL_BACKOFF"]=value
            subprocess.run([str(exe)],env=env,check=True)
        for value in ("0","1"):
            env=os.environ.copy();env["ACV_GPU_POLL_BACKOFF"]=value
            result=subprocess.run([str(exe),"benchmark"],env=env,text=True,capture_output=True,check=True)
            results.append(json.loads(result.stdout))
        assert results[1]["cpu_ms"]<results[0]["cpu_ms"]*.5,results
        print(json.dumps({"scope":"Actual hook + ABI stub; host polling only, not GPU/game FPS",
                          "cases":"pending/completed/caller/exception/context/env",
                          "benchmark":results}))
if __name__=="__main__":main()
