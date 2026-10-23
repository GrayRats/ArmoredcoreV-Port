#!/usr/bin/env python3
"""Run actual SPIR-V exponent-emission fragment through a semantic builder mock."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/"tools/rexglue-sdk/src/graphics/pipeline/shader/spirv_translator_fetch.cpp").read_text()
start=source.index("        id_vector_temp_.clear();",source.index("// Result exponent bias is bits"))
end=source.index("        {",start)
fragment=source[start:end]
harness=r'''#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdio>
namespace spv {enum Op {OpBitcast,OpBitFieldSExtract};constexpr int StorageClassUniform=0,NoPrecision=0;struct Id {uint32_t u=0;float f=0;};}
constexpr int GLSLstd450Ldexp=0;
struct Builder {
 std::array<uint32_t,192> words{};
 spv::Id makeIntConstant(int n){return {uint32_t(n),0};}
 spv::Id makeUintConstant(unsigned n){return {n,0};}
 spv::Id createAccessChain(int,int,const std::vector<spv::Id>& a){return {words.at(a.at(1).u*4+a.at(2).u),0};}
 spv::Id createLoad(spv::Id x,int){return x;}
 spv::Id createUnaryOp(spv::Op,int,spv::Id x){return x;}
 spv::Id createTriOp(spv::Op,int,spv::Id x,spv::Id offset,spv::Id width){
  unsigned n=(x.u>>offset.u)&((1u<<width.u)-1);int v=int(n);if(n&(1u<<(width.u-1)))v-=1u<<width.u;return {uint32_t(v),0};
 }
 spv::Id createBinBuiltinCall(int,int,int,spv::Id x,spv::Id e){return {0,std::ldexp(x.f,int32_t(e.u))};}
};
float emitted(Builder* builder_,unsigned fetch_constant_word_0_index){
 std::vector<spv::Id> id_vector_temp_;spv::Id const_int_0_{},const_float_1_{0,1};
 int uniform_fetch_constants_=0,type_float_=0,type_int_=0,ext_inst_glsl_std_450_=0;
 @FRAGMENT@
 return result_exponent_bias.f;
}
int main(){
 Builder b;unsigned checks=0;
 // dword4's LOD bits deliberately disagree with every expected result exponent.
 for(unsigned slot=0;slot<32;++slot)for(int exponent=-32;exponent<32;++exponent){
  b.words.fill(0x3e0803);b.words[slot*6+3]=0x06a80d10u | ((uint32_t(exponent)&63)<<13);
  b.words[slot*6+4]=((uint32_t(exponent+1)&63)<<13);
  if(emitted(&b,slot*6)!=std::ldexp(1.f,exponent))return 1;
  ++checks;
 }
 b.words[3]=0x06a80d10;b.words[4]=0x003e0803;
 if(emitted(&b,0)!=1.f)return 2;
 std::printf("PASS: %u signed exponent/slot cases plus captured ACV descriptor; LOD word does not affect sampled color\n",checks);
}
'''
with tempfile.TemporaryDirectory(prefix="acv-fetch-exp-") as temporary:
 p=Path(temporary);cpp=p/"test.cpp";exe=p/"test"
 def build(fragment):
  cpp.write_text(harness.replace("@FRAGMENT@",fragment))
  subprocess.run(["clang++","-std=c++23","-fsanitize=address,undefined",str(cpp),"-o",str(exe)],check=True)
 build(fragment)
 subprocess.run([str(exe)],check=True)
 bad=fragment.replace("fetch_constant_word_0_index + 3","fetch_constant_word_0_index + 4")
 assert bad!=fragment
 build(bad)
 result=subprocess.run([str(exe)],capture_output=True)
 assert result.returncode==1,"Old wrong-word behavior must fail"
 print("PASS: negative control rejects dword4 regression")
