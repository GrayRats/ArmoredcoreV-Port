#!/usr/bin/env python3
"""Actual FOV/HUD hooks: 2560x1080 projection and centre/proportion boundary tests only."""
import os, subprocess, tempfile
from pathlib import Path
SOURCE=Path(__file__).with_name("scene_aspect_experiment.cpp")
HEADER=r"""
#pragma once
#include <cstdint>
#include <cstring>
union Fpr {double f64;uint64_t bits;};
union Gpr {uint64_t u64;uint32_t u32;};
struct PPCContext {uint64_t lr; Fpr f1,f2,f3,f4,f5,f6;Gpr r3,r8,r9;};
inline uint32_t test_load_u32(uint8_t* base,uint32_t address) {uint32_t v;std::memcpy(&v,base+address,4);return __builtin_bswap32(v);}
inline void test_store_u32(uint8_t* base,uint32_t address,uint32_t v) {v=__builtin_bswap32(v);std::memcpy(base+address,&v,4);}
#define REX_LOAD_U32(address) test_load_u32(base,address)
#define REX_STORE_U32(address,value) test_store_u32(base,address,value)
#define REX_EXTERN(name) extern "C" void name(PPCContext& ctx,uint8_t* base)
"""
TEST=r"""
#include "acv_linux_probe_pch.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>
REX_EXTERN(sub_82BC8980); REX_EXTERN(sub_82BC8CC0);
static std::array<double,6> seen;
REX_EXTERN(__imp__sub_82302520) {}
static unsigned calls=0; static bool throwing=false;
REX_EXTERN(__imp__sub_82BC8980) {
  ++calls;seen={ctx.f1.f64,ctx.f2.f64,ctx.f3.f64,ctx.f4.f64,0,0};ctx.r3.u64=71;
  if(throwing)throw std::runtime_error("original perspective");
}
REX_EXTERN(__imp__sub_82BC8CC0) {
  ++calls;seen={ctx.f1.f64,ctx.f2.f64,ctx.f3.f64,ctx.f4.f64,ctx.f5.f64,ctx.f6.f64};ctx.r3.u64=73;
  if(throwing)throw std::runtime_error("original ortho");
}
bool close(double a,double b){return std::abs(a-b)<1e-9;}
int main(int argc,char** argv) {
  const bool enabled=argc==2 && std::strcmp(argv[1],"enabled")==0;
  constexpr double target=2560.0/1080.0, native=16.0/9.0;
  PPCContext c{};
  for(uint32_t site:{0x822B4510u,0x82300978u,0x822B450Cu}) {
    for(double fov:{0.4,0.7853981633974483,1.0471975511965976,1.5}) {
      c.lr=site;c.f1.f64=fov;c.f2.f64=native;c.f3.f64=1;c.f4.f64=1000;
      const unsigned old=calls;sub_82BC8980(c,nullptr);
      assert(calls==old+1 && c.r3.u64==71 && c.lr==site);
      assert(c.f1.f64==fov && c.f2.f64==native);
      const bool selected=enabled && site!=0x822B450C;
      assert(close(seen[0],fov) && close(seen[1],selected?target:native));
      assert(seen[2]==1 && seen[3]==1000);
      if(selected) {
        const double y=1/std::tan(seen[0]/2),x=y/seen[1];
        assert(close(x*2560/2,y*1080/2)); // Equal physical scale: no scene stretch.
        assert(2*std::atan(std::tan(fov/2)*target)>2*std::atan(std::tan(fov/2)*native));
      }
    }
  }
  c.lr=0x82300978;c.f1.f64=1;c.f2.f64=2;
  sub_82BC8980(c,nullptr);assert(seen[1]==2); // Foreign 2:1 pass is untouched.
  c.lr=0x822B4510;c.f1.f64=0;c.f2.f64=native;
  sub_82BC8980(c,nullptr);assert(seen[0]==0 && seen[1]==native);
  c.lr=0x822B4510;c.f1.f64=1;c.f2.f64=native;throwing=true;
  try{sub_82BC8980(c,nullptr);assert(false);}catch(const std::runtime_error&){}
  assert(c.f1.f64==1 && c.f2.f64==native && c.r3.u64==71);throwing=false;
  auto hud=[&](uint32_t site,double right,double bottom) {
    c.lr=site;c.f1.f64=0;c.f2.f64=right;c.f3.f64=bottom;c.f4.f64=0;c.f5.f64=1;c.f6.f64=10000;
    const unsigned old=calls;sub_82BC8CC0(c,nullptr);
    assert(calls==old+1 && c.r3.u64==73);
    assert(c.f1.f64==0 && c.f2.f64==right && c.f3.f64==bottom && c.f4.f64==0);
    assert(c.f5.f64==1 && c.f6.f64==10000);
  };
  hud(0x822B5368,1280,-720);
  if(enabled) {
    assert(close(seen[0],-640.0/3) && close(seen[1],4480.0/3));
    auto sx=[&](double x){return 2560*(x-seen[0])/(seen[1]-seen[0]);};
    auto sy=[&](double y){return 1080*(seen[3]-y)/(seen[3]-seen[2]);};
    assert(close(sx(640),1280) && close(sy(-360),540)); // Reticle centre.
    assert(close(sx(740)-sx(640),sy(-460)-sy(-360))); // HUD circle stays circular.
    assert(close(sx(0),320) && close(sx(1280),2240)); // Centred 1920-wide safe area.
  }else assert(seen[0]==0 && seen[1]==1280);
  hud(0x822B5364,1280,-720);assert(seen[0]==0 && seen[1]==1280);
  hud(0x822B5368,1280,0);assert(seen[0]==0 && seen[1]==1280); // Degenerate bounds.
  c.lr=0x822B5368;c.f1.f64=0;c.f2.f64=1280;c.f3.f64=-720;c.f4.f64=0;
  throwing=true;
  try{sub_82BC8CC0(c,nullptr);assert(false);}catch(const std::runtime_error&){}
  assert(c.f1.f64==0 && c.f2.f64==1280 && c.f3.f64==-720 && c.f4.f64==0 && c.r3.u64==73);
}
"""
def main():
    with tempfile.TemporaryDirectory(prefix="acv-fov-hud-") as temp:
        root=Path(temp);(root/"rex").mkdir()
        (root/"acv_linux_probe_pch.h").write_text(HEADER)
        (root/"rex/logging.h").write_text("#pragma once\n#define REXLOG_INFO(...) ((void)0)\n#define REXLOG_WARN(...) ((void)0)\n")
        (root/"test.cpp").write_text(TEST)
        exe=root/"test"
        subprocess.run(["clang++","-std=c++23","-O2","-g","-pthread","-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer","-I",str(root),str(SOURCE),str(root/"test.cpp"),"-o",str(exe)],check=True)
        for value,enabled in [("",False),("2.3703703703703702",True),("nan",False),("2.37junk",False)]:
            env=os.environ.copy()
            env.update(ACV_EXPERIMENTAL_SCENE_ASPECT=value,ACV_EXPERIMENTAL_HUD_ASPECT=value,ACV_PROJECTION_CENSUS="1")
            subprocess.run([str(exe),*(["enabled"] if enabled else [])],env=env,check=True)
    print("PASS actual FOV/HUD hooks ASan/UBSan: 2560x1080 camera isotropy, horizontal coverage, HUD centre/safe area, pass-through, return values, exceptions. Other resolutions and CAS/FSR not tested.")
if __name__=="__main__":main()
