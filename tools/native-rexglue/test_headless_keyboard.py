#!/usr/bin/env python3
"""Compile and exercise the actual SDK headless keyboard copy lambda."""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument("--baseline",type=Path);args=p.parse_args()
root=Path(__file__).resolve().parents[2]
s=(root/"tools/rexglue-sdk/src/kernel/xam/xam_ui.cpp").read_text()
a=s.index("    auto run = [default_text, buffer, buffer_length, buffer_size]()")
b=s.index("    result = xeXamDispatchHeadless",a)
body=args.baseline.read_text() if args.baseline else s[a:b]
harness=r'''#include <rex/string.h>
#include <rex/types.h>
#include <array>
#include <cassert>
#include <cstdio>
#include "@HEADER@"
using X_RESULT=uint32_t;
constexpr uint32_t X_ERROR_SUCCESS=0;
struct Wide : rex::MappedPtr<char16_t> { Wide(char16_t* p):MappedPtr(p,p ? 1 : 0){} };
void copy(Wide default_text,Wide buffer,uint32_t buffer_length){
size_t buffer_size=buffer_length*2;
@BODY@
assert(run()==X_ERROR_SUCCESS);
}
int main(){
 alignas(2) std::array<uint8_t,32> source={0,65,0,82,0,84,0,69,0,77,0,0};
 auto expected=source;
 copy({reinterpret_cast<char16_t*>(source.data())},{reinterpret_cast<char16_t*>(source.data())},16);
 if(source!=expected){std::puts("FAIL: in-place guest UTF-16 bytes swapped");return 1;}
 std::array<uint8_t,32> dest;dest.fill(0xcd);
 copy({reinterpret_cast<char16_t*>(source.data())},{reinterpret_cast<char16_t*>(dest.data())},16);
 assert(std::memcmp(source.data(),dest.data(),12)==0);
 assert(dest[12]==0xcd);
 copy({nullptr},{reinterpret_cast<char16_t*>(dest.data())},16);
 for(auto b:dest)assert(b==0);
 alignas(2) uint8_t pair[]={0,65,0xd8,0x3d,0xde,0,0,0};
 dest.fill(0xcd);
 copy({reinterpret_cast<char16_t*>(pair)},{reinterpret_cast<char16_t*>(dest.data())},3);
 assert(dest[0]==0 && dest[1]==65 && dest[2]==0 && dest[3]==0 && dest[6]==0xcd);
 std::puts("PASS: actual headless lambda, in-place/disjoint BE copy, empty input, surrogate truncation and guards");
}
'''
with tempfile.TemporaryDirectory(prefix="acv-headless-keyboard-") as d:
 d=Path(d);cpp=d/"test.cpp";exe=d/"test";cpp.write_text(harness.replace("@HEADER@",str(root/"tools/native-rexglue/profile_text.h")).replace("@BODY@",body))
 subprocess.run(["clang++","-std=c++23","-O1","-g","-fsanitize=address,undefined","-DSPDLOG_FMT_EXTERNAL","-DSPDLOG_COMPILED_LIB","-I"+str(root/"tools/rexglue-sdk/include"),"-isystem","/home/art/.local/opt/rexglue-sdk/0.10.0/include",str(cpp),"-L"+str(root/"tmp/acv-mission2-join-fix"),"-Wl,-rpath,"+str(root/"tmp/acv-mission2-join-fix"),"-lrexruntime","-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
