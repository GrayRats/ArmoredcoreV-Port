#pragma once
#include "../bake_chain_trace.h"
#include <unordered_set>
namespace {
void AcvTraceResolvePublish(uint32_t physical,uint32_t bytes,uint64_t ticket,
                           uint64_t completed,uint64_t frame,const uint8_t* data) {
  static AcvBakeChainTrace trace("gpu");
  if(!trace.enabled())return;
  struct Filter {
    uint64_t start=0,end=uint64_t(1)<<29;
    bool valid=true;
    Filter() {
      const char* first=std::getenv("ACV_BAKE_TRACE_PHYSICAL_START");
      const char* count=std::getenv("ACV_BAKE_TRACE_PHYSICAL_LENGTH");
      if(!first && !count)return;
      if(!first || !count) {valid=false;return;}
      auto parse=[](const char* text,uint64_t& value) {
        char* tail=nullptr;errno=0;value=std::strtoull(text,&tail,0);
        return !errno && tail!=text && !*tail && *text!='-';
      };
      uint64_t length=0;
      valid=parse(first,start) && parse(count,length) && length &&
          start<(uint64_t(1)<<29) && length<=(uint64_t(1)<<29)-start;
      if(valid)end=start+length;
      else std::fprintf(stderr,"[acv-bake] Invalid physical trace range; disabled\n");
    }
  };
  static std::unordered_set<uint64_t> observed;
  const uint64_t key=(uint64_t(physical)<<32)|bytes;
  if(observed.size()<128 && observed.insert(key).second)
    trace.Record("\"event\":\"resolve_range\",\"physical\":%u,\"bytes\":%u,\"ticket\":%llu,\"frame\":%llu",
        physical,bytes,(unsigned long long)ticket,(unsigned long long)frame);
  static const Filter filter;
  if(!filter.valid || uint64_t(physical)>=filter.end ||
      uint64_t(physical)+bytes<=filter.start)return;
  static bool source_saved=false;
  if(bytes==0x200000 && !source_saved) {
    source_saved=true;AcvBakeSnapshot(".resolve-first-2m.bin",data,bytes);
  }
  trace.Record("\"event\":\"resolve_publish\",\"physical\":%u,\"bytes\":%u,"
      "\"ticket\":%llu,\"completed\":%llu,\"frame\":%llu,\"sampled\":%u,"
      "\"sample_fnv1a64\":\"%016llx\"",physical,bytes,
      (unsigned long long)ticket,(unsigned long long)completed,
      (unsigned long long)frame,std::min(bytes,4096u),
      (unsigned long long)AcvBakeSampleHash(data,bytes));
}
} // namespace
