#!/usr/bin/env python3
# Exercise the exact generated leaf confirmed against raw retail PPC.
import json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
source=ROOT/'tmp/acv-rexglue-linux-indexed/generated/conditional-fragments/acv_linux_probe_recomp.275.cpp'
s=source.read_text();a=s.index('DEFINE_REX_FUNC(sub_82C9AF88)');b=s.find('DEFINE_REX_FUNC(',a+25);body=s[a:b]
# The runtime snapshot proves U+4100 has a null page. The ASCII path does not
# access any mapping page. No invented mapping is used for either input.
proof=json.loads((ROOT/'logs/acv-name-chain-20261025/codec-core.json').read_text())
assert bytes.fromhex(proof['codec_globals'])[9]==3
assert proof['0x4100']['page']=='0x0'
harness=r'''#include <rex/ppc/context.h>
#include <sys/mman.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#define DEFINE_REX_FUNC(name) void name(PPCContext& ctx,uint8_t* base)
#define REX_FUNC_PROLOGUE() ((void)0)
#define REX_LOAD_U8(x) (*(volatile uint8_t*)(base+uint32_t(x)))
#define REX_LOAD_U16(x) __builtin_bswap16(*(volatile uint16_t*)(base+uint32_t(x)))
#define REX_LOAD_U32(x) __builtin_bswap32(*(volatile uint32_t*)(base+uint32_t(x)))
#define REX_LOAD_U64(x) __builtin_bswap64(*(volatile uint64_t*)(base+uint32_t(x)))
#define REX_STORE_U8(x,y) (*(volatile uint8_t*)(base+uint32_t(x))=(y))
#define REX_STORE_U32(x,y) (*(volatile uint32_t*)(base+uint32_t(x))=__builtin_bswap32(uint32_t(y)))
#define REX_STORE_U64(x,y) (*(volatile uint64_t*)(base+uint32_t(x))=__builtin_bswap64(uint64_t(y)))
@BODY@
int main(){
 auto* base=static_cast<uint8_t*>(mmap(nullptr,0x100000000ull,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0));assert(base!=MAP_FAILED);
 for(bool little:{false,true}){
  PPCContext ctx{};ctx.r1.u32=0x10000;ctx.r3.u32=0x20000;ctx.r4.u32=64;ctx.r5.u32=0x30000;ctx.r6.u32=0x40000;
  memset(base+0x20000,0xcc,64);
  const char* name="ARTEM";for(int i=0;i<5;i++){base[0x30000+i*2]=little?name[i]:0;base[0x30001+i*2]=little?0:name[i];}
  REX_STORE_U32(0x40000,5);
  sub_82C9AF88(ctx,base);
  if(little){assert(ctx.r3.u32==0);assert(REX_LOAD_U32(0x40000)==0);for(int i=0;i<64;i++)assert(base[0x20000+i]==0xcc);}
  else{assert(ctx.r3.u32==5);assert(REX_LOAD_U32(0x40000)==5);assert(memcmp(base+0x20000,"ARTEM",5)==0);assert(base[0x20005]==0xcc);}
 }
 munmap(base,0x100000000ull);
 puts("PASS retail-backed Shift-JIS leaf: BE ARTEM converts to 5 bytes; old LE spelling stops at unmapped U+4100 with zero output/consumption");
}
'''
with tempfile.TemporaryDirectory(prefix='acv-name-codec-') as temporary:
 d=Path(temporary);cpp=d/'codec.cpp';exe=d/'codec';cpp.write_text(harness.replace('@BODY@',body))
 subprocess.run(['c++','-std=c++23','-O1','-g','-fsanitize=undefined','-I'+str(ROOT/'tools/rexglue-sdk/include'),'-I'+str(ROOT/'tools/rexglue-sdk/thirdparty/simde'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
