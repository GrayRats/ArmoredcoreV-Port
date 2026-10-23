// Profile strings arrive as UTF-8; guest names are UTF-16 code units.
// Preserve the existing '?' replacement policy for malformed input. Valid
// supplementary scalars use a pair and are never split at the capacity bound.
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
namespace acv::profile {
// A UTF-16 code unit needs at most three UTF-8 bytes (a supplementary scalar
// uses four bytes for two units). Keep room for the ImGui NUL terminator.
inline size_t Utf8BufferBytesForUtf16Units(size_t units) {
  if(units>(std::numeric_limits<size_t>::max()-1)/3)
    throw std::length_error("UTF-16 input capacity overflows UTF-8 buffer size");
  return units*3+1;
}

inline std::u16string DecodeUtf8(std::string_view text, size_t max_units) {
  std::u16string out;
  for(size_t i=0;i<text.size() && out.size()<max_units;) {
    const auto lead=static_cast<uint8_t>(text[i]);
    if(!lead)break;
    uint32_t value=0,minimum=0;size_t count=0;
    if(lead<0x80) {value=lead;count=1;}
    else if(lead>=0xc2 && lead<=0xdf) {value=lead&0x1f;minimum=0x80;count=2;}
    else if(lead>=0xe0 && lead<=0xef) {value=lead&0x0f;minimum=0x800;count=3;}
    else if(lead>=0xf0 && lead<=0xf4) {value=lead&7;minimum=0x10000;count=4;}
    bool valid=count && count<=text.size()-i;
    if(valid)for(size_t j=1;j<count;++j) {
      const auto next=static_cast<uint8_t>(text[i+j]);
      if((next&0xc0)!=0x80) {valid=false;break;}
      value=(value<<6)|(next&0x3f);
    }
    valid=valid && value>=minimum && value<=0x10ffff &&
        !(value>=0xd800 && value<=0xdfff);
    if(!valid) {out.push_back(u'?');++i;continue;}
    const size_t units=value>0xffff ? 2 : 1;
    if(units>max_units-out.size())break;
    if(units==1)out.push_back(static_cast<char16_t>(value));
    else {
      value-=0x10000;
      out.push_back(static_cast<char16_t>(0xd800+(value>>10)));
      out.push_back(static_cast<char16_t>(0xdc00+(value&0x3ff)));
    }
    i+=count;
  }
  return out;
}
} // namespace acv::profile
