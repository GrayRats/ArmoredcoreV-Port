#!/usr/bin/env python3
"""Test actual profile decoder and guest UTF-16 store under ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/"tools/native-rexglue/acv_compat_shims.cpp").read_text()
a=source.index("void StoreGuestWide(");b=source.index("\n}  // namespace",a)
store=source[a:b]
harness=r'''#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include "@HEADER@"
#define REX_STORE_U16(a,v) do {base[a]=uint8_t(uint16_t(v)>>8);base[(a)+1]=uint8_t(v);}while(false)
@STORE@
std::string encode(uint32_t c){
 std::string s;
 if(c<128)s+=char(c);
 else if(c<2048){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}
 else if(c<65536){s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 else{s+=char(0xf0|(c>>18));s+=char(0x80|((c>>12)&63));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 return s;
}
int main(){
 using acv::profile::DecodeUtf8;
 assert(acv::profile::Utf8BufferBytesForUtf16Units(0)==1);
 assert(acv::profile::Utf8BufferBytesForUtf16Units(1)==4);
 assert(acv::profile::Utf8BufferBytesForUtf16Units(2)==7);
 bool overflow=false;try{acv::profile::Utf8BufferBytesForUtf16Units(SIZE_MAX);}catch(const std::length_error&){overflow=true;}assert(overflow);
 assert(DecodeUtf8("ARTEM",15)==u"ARTEM");
 assert(DecodeUtf8("\xd0\x90\xd1\x80\xd1\x82\xd1\x91\xd0\xbc",15)==u"Артём");
 assert(DecodeUtf8("\xf0\x9f\x98\x80",2)==u"\U0001f600");
 assert(DecodeUtf8("A\xf0\x9f\x98\x80",2)==u"A");
 assert(DecodeUtf8("\xc0\x80",8)==u"??");
 assert(DecodeUtf8("\xe0\x80\x80",8)==u"???");
 assert(DecodeUtf8("\xed\xa0\x80",8)==u"???");
 assert(DecodeUtf8("\xf4\x90\x80\x80",8)==u"????");
 assert(DecodeUtf8("\xe2(\xa1",8)==u"?(?");
 assert(DecodeUtf8("\xf0\x90\x80",8)==u"???");
 // All valid non-NUL Unicode scalars, including both sides of every boundary.
 unsigned checked=0;
 for(uint32_t c=1;c<=0x10ffff;++c){
  if(c>=0xd800 && c<=0xdfff)continue;
  auto decoded=DecodeUtf8(encode(c),2);
  if(c<0x10000)assert(decoded.size()==1 && decoded[0]==c);
  else{uint32_t q=c-0x10000;assert(decoded.size()==2 && decoded[0]==0xd800+(q>>10) && decoded[1]==0xdc00+(q&1023));assert(DecodeUtf8(encode(c),1).empty());}
  ++checked;
 }
 for(unsigned cap=0;cap<=16;++cap){
  std::array<uint8_t,80> memory;memory.fill(0xcd);auto* base=memory.data();
  auto text=DecodeUtf8("ARTEM\xf0\x9f\x98\x80",cap ? cap-1 : 0);
  StoreGuestWide(base,8,cap,text);
  for(unsigned i=0;i<8;++i)assert(memory[i]==0xcd);
  for(unsigned i=8+cap*2;i<memory.size();++i)assert(memory[i]==0xcd);
  if(cap){
   for(unsigned i=0;i<text.size();++i)assert(memory[8+i*2]==uint8_t(text[i]>>8) && memory[9+i*2]==uint8_t(text[i]));
   assert(memory[8+text.size()*2]==0 && memory[9+text.size()*2]==0);
  }
 }
 std::printf("PASS: %u Unicode scalars, Cyrillic, malformed/truncated UTF-8, UTF-16/UTF-8 buffer sizing, pair capacity, actual guest big-endian store and NUL/guard bounds\n",checked);
}
'''
with tempfile.TemporaryDirectory(prefix="acv-profile-text-") as temporary:
 p=Path(temporary);cpp=p/"test.cpp";exe=p/"test"
 cpp.write_text(harness.replace("@HEADER@",str(root/"tools/native-rexglue/profile_text.h")).replace("@STORE@",store))
 subprocess.run(["clang++","-std=c++23","-O1","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer",str(cpp),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
