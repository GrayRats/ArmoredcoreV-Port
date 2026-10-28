#!/usr/bin/env python3
"""Actual hook plus translated retail sphere culling, SDK PPC context, target2560x1080."""
import hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SDK=ROOT/'tools/rexglue-sdk'
SOURCE=Path(__file__).with_name('scene_aspect_experiment.cpp')
EVIDENCE=ROOT/'logs/acv-engine-offscreen-20261028'
HEADER=r"""
#pragma once
#include <rex/ppc/context.h>
#include <rex/ppc/intrinsics.h>
#include <cstdint>
#include <cstring>
inline uint32_t test_offset(uint32_t a){return a>=0x82000000u ? 0x10000u+(a&0xffffu) : a;}
inline uint32_t test_load_u32(uint8_t* base,uint32_t a){uint32_t v;std::memcpy(&v,base+test_offset(a),4);return __builtin_bswap32(v);}
inline uint64_t test_load_u64(uint8_t* base,uint32_t a){uint64_t v;std::memcpy(&v,base+test_offset(a),8);return __builtin_bswap64(v);}
inline void test_store_u32(uint8_t* base,uint32_t a,uint32_t v){v=__builtin_bswap32(v);std::memcpy(base+test_offset(a),&v,4);}
#define REX_LOAD_U32(a) test_load_u32(base,uint32_t(a))
#define REX_LOAD_U64(a) test_load_u64(base,uint32_t(a))
#define REX_STORE_U32(a,v) test_store_u32(base,uint32_t(a),uint32_t(v))
#define REX_RAW_ADDR(a) (base+test_offset(uint32_t(a)))
#define REX_FUNC_PROLOGUE() ((void)0)
#define DEFINE_REX_FUNC(name) REX_EXTERN(name)
REX_EXTERN(sub_823009C8);
"""
TEST=r"""
#include "acv_linux_probe_pch.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
REX_EXTERN(sub_82302520);REX_EXTERN(sub_82300CE0);REX_EXTERN(sub_82BC8980);
REX_EXTERN(__imp__sub_82BC8980){} REX_EXTERN(__imp__sub_82BC8CC0){}
constexpr uint32_t camera=0x1000;
alignas(64) static std::array<uint8_t,0x30000> bytes{};
static std::array<uint8_t,0x260> retail{};
static bool throwing=false,corrupt=false;
static unsigned calls=0;static uint32_t projection_site=0x82300978;
static uint32_t original_width=1280,original_height=720;
void put(uint8_t* base,uint32_t a,float f){test_store_u32(base,a,std::bit_cast<uint32_t>(f));}
float get(uint8_t* base,uint32_t a){return std::bit_cast<float>(test_load_u32(base,a));}
REX_EXTERN(__imp__sub_82302520){
 ++calls;
 if(throwing)throw std::runtime_error("original builder failed");
 if(!base)return;
 const uint32_t a=ctx.r3.u32;
 for(unsigned i=0;i<0x260;i++)base[a+i]=0;
 for(unsigned i=0;i<4;i++)put(base,a+i*20,1); // Identity world->camera transform.
 const double fov=ctx.f1.f64,ratio=double(ctx.r8.u32)/ctx.r9.u32;
 PPCContext projection=ctx;projection.lr=projection_site;projection.f2.f64=ratio;sub_82BC8980(projection,base);
 const double tangent=std::tan(fov/2), slope=tangent*ratio;
 const float x=float(1/std::sqrt(1+slope*slope)),z=float(-slope/std::sqrt(1+slope*slope));
 put(base,a+0x40,float(fov));put(base,a+0x44,1);put(base,a+0x48,1000);
 test_store_u32(base,a+0x4C,ctx.r8.u32);test_store_u32(base,a+0x50,ctx.r9.u32);
 put(base,a+0x60,-x);put(base,a+0x68,z);put(base,a+0x70,x);put(base,a+0x78,z);
 put(base,a+0x84,float(1/std::sqrt(1+tangent*tangent)));put(base,a+0x88,float(-tangent/std::sqrt(1+tangent*tangent)));
 put(base,a+0x94,-get(base,a+0x84));put(base,a+0x98,get(base,a+0x88));
 // Distinct sentinel bytes outside the local planes, including derived world planes.
 for(unsigned i=0x200;i<0x260;i++)base[a+i]=uint8_t(i^0x5A);
 if(corrupt)put(base,a+0x74,.2f);
 std::memcpy(retail.data(),base+a,retail.size());
 ctx.r3.u64=0x55;ctx.r8.u64=0x66;ctx.r9.u64=0x77;ctx.f1.f64=3.0;
}
bool visible(double x,double y,double z,double radius=0){
 PPCContext c{};c.fpscr.InitHost();c.r3.u32=camera;c.r1.u32=0x8000;c.lr=0x1234;c.f1.f64=radius;
 c.v1.f32[3]=float(x);c.v1.f32[2]=float(y);c.v1.f32[1]=float(z);c.v1.f32[0]=1;
 sub_82300CE0(c,bytes.data());return c.r3.u32!=0;
}
void build(uint32_t caller,double fov=1.0){
 PPCContext c{};c.lr=caller;c.r3.u32=camera;c.r8.u32=original_width;c.r9.u32=original_height;c.f1.f64=fov;
 const unsigned previous=calls;sub_82302520(c,bytes.data());
 assert(calls==previous+1 && c.r3.u64==0x55 && c.r8.u64==0x66 && c.r9.u64==0x77 && c.f1.f64==3.0 && c.lr==caller);
}
int main(int argc,char** argv){
 assert(argc==3);const bool enabled=std::string(argv[1])=="enabled";
 std::ifstream fixture(argv[2],std::ios::binary);fixture.read(reinterpret_cast<char*>(bytes.data()+0x10000),0x10000);assert(fixture.gcount()==0x10000);
 constexpr double target=2560.0/1080.0,native=16.0/9.0;
 for(uint32_t caller:{0x823031CCu,0x82B0E974u,0x82303164u,0x1234u})for(double fov:{0.52,0.7853981633974483,1.0471975511965976}){
  build(caller,fov);
  for(unsigned i=0;i<retail.size();i++)if(!(i>=0x60 && i<0x64) && !(i>=0x68 && i<0x6C) && !(i>=0x70 && i<0x74) && !(i>=0x78 && i<0x7C))assert(bytes[camera+i]==retail[i]);
  assert(test_load_u32(bytes.data(),camera+0x4C)==1280 && test_load_u32(bytes.data(),camera+0x50)==720);
  const double old_edge=100*std::tan(fov/2)*native, new_edge=100*std::tan(fov/2)*target;
  assert(visible(0,0,100));assert(!visible(0,0,.5));assert(!visible(0,0,1100));
  for(double sign:{-1.0,1.0}){
   assert(visible(sign*old_edge*.99,0,100));
   assert(visible(sign*(old_edge+new_edge)/2,0,100)==enabled);
   assert(!visible(sign*new_edge*1.02,0,100));
   assert(visible(sign*(enabled?new_edge:old_edge)*1.005,0,100,1));
  }
  assert(!visible(0,100*std::tan(fov/2)*1.02,100)); // Vertical clipping unchanged.
  const float x=get(bytes.data(),camera+0x60),z=get(bytes.data(),camera+0x68);
  assert(std::abs(x*x+z*z-1)<1e-5);
 }
 projection_site=0x1234;build(0x1234);assert(std::memcmp(bytes.data()+camera,retail.data(),retail.size())==0);projection_site=0x82300978;
 original_width=1440; // Foreign2:1 passes are not changed; no runtime resolution test.
 build(0x823031CC);assert(std::memcmp(bytes.data()+camera,retail.data(),retail.size())==0);original_width=1280;
 corrupt=true;build(0x823031CC);assert(std::memcmp(bytes.data()+camera,retail.data(),retail.size())==0);corrupt=false;
 throwing=true;PPCContext c{};c.r3.u32=camera;c.r8.u32=1280;c.r9.u32=720;c.f1.f64=1;c.lr=0x823031CC;
 const auto before=bytes;try{sub_82302520(c,bytes.data());assert(false);}catch(const std::runtime_error&){}assert(bytes==before);
 std::puts("PASS: actual translated retail sphere culling; UltraWide side-band, far/near/vertical limits, sphere radius, normalized planes, caller ABI, all unrelated bytes and exceptions");
}
"""
def extract(shard,name):
    text=(ROOT/f'tmp/acv-rexglue-linux-indexed/generated/default/acv_linux_probe_recomp.{shard}.cpp').read_text()
    begin=text.index(f'DEFINE_REX_FUNC(sub_{name})');end=text.find('\nDEFINE_REX_FUNC(',begin+1)
    return text[begin:end if end>=0 else len(text)]
def main():
    captures=json.loads((EVIDENCE/'retail-data.json').read_text())
    fixture=bytearray(0x10000)
    for text,row in captures.items():
        start=int(text,16)&0xffff;data=bytes.fromhex(row['hex']);fixture[start:start+len(data)]=data
    functions=[extract('223','823009C8'),extract('306','82300CE0')]
    with tempfile.TemporaryDirectory(prefix='acv-native-frustum-') as temp:
        root=Path(temp);(root/'rex').mkdir();(root/'acv_linux_probe_pch.h').write_text(HEADER)
        (root/'rex/logging.h').write_text('#pragma once\n#define REXLOG_INFO(...) ((void)0)\n#define REXLOG_WARN(...) ((void)0)\n')
        (root/'retail.cpp').write_text('#include "acv_linux_probe_pch.h"\n'+'\n'.join(functions));(root/'test.cpp').write_text(TEST)
        (root/'fixture.bin').write_bytes(fixture);exe=root/'test'
        subprocess.run(['clang++','-std=c++23','-O2','-g','-pthread','-msse4.1','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-I',str(root),'-I',str(SDK/'include'),'-I',str(SDK/'thirdparty/simde'),str(SOURCE),str(root/'retail.cpp'),str(root/'test.cpp'),'-o',str(exe)],check=True)
        for aspect,sync,on in [('2.3703703703703702','1',True),('2.3703703703703702','0',False),('','1',False),('nan','1',False)]:
            env=dict(os.environ,ACV_EXPERIMENTAL_SCENE_ASPECT=aspect,ACV_NATIVE_FRUSTUM_SYNC=sync,ACV_FRUSTUM_TRACE='0')
            subprocess.run([str(exe),'enabled' if on else 'disabled',str(root/'fixture.bin')],env=env,check=True)
    result={'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'retail_function_sha256':[hashlib.sha256(x.encode()).hexdigest() for x in functions],
            'scope':'actual translated retail sphere tests using SDK PPC context and captured data; target2560x1080. Original camera builder is a shaped fixture, not the retail implementation. No gameplay/retention/performance acceptance.'}
    (EVIDENCE/'host-frustum-test.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
