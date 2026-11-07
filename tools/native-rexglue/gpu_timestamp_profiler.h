#pragma once
// Bounded diagnostic only. Include after SDK Vulkan API feature macros.
// Regions are disjoint command ranges, not exclusive GPU hardware time. TOP / 
// BOTTOM timestamps may include pipeline overlap and implementation-later stages.
#include <vulkan/vulkan_core.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace acv {
class GpuTimestampProfiler {
 public:
  static constexpr uint32_t kSlots=64,kRegions=16,kQueriesPerSlot=kRegions*2,kQueries=kSlots*kQueriesPerSlot;
  static constexpr uint64_t kMaxSamples=1200;
  enum class Phase { Free,Recording,Recorded,Submitted,Failed };
  struct Region { uint32_t category=0;uint64_t commands=0,copy_bytes=0; };
  struct Record {
    Phase phase=Phase::Free;uint64_t ticket=0;double age=0;
    uint32_t regions=0,attempts=0;bool open=false,truncated=false;
    std::array<Region,kRegions> spans{};
  };
  struct Value { uint64_t ticks=0,available=0; };
  struct Stats {
    uint64_t eligible=0,selected=0,submitted=0,completed=0,emitted=0;
    uint64_t capacity_dropped=0,truncated=0,not_ready=0,discarded=0,failed=0,errors=0,ambiguous=0;
  };
  struct Api { PFN_vkCreateQueryPool create;PFN_vkGetQueryPoolResults get;PFN_vkDestroyQueryPool destroy; };
  GpuTimestampProfiler(Api api,VkDevice device,uint32_t bits,double period)
      :api_(api),device_(device),bits_(bits),period_(period) {
    if(!bits || bits>64 || !std::isfinite(period) || period<=0) return;
    if(!api_.create || !api_.get || !api_.destroy){last_result_=VK_ERROR_INITIALIZATION_FAILED;return;}
    VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};ci.queryType=VK_QUERY_TYPE_TIMESTAMP;ci.queryCount=kQueries;
    last_result_=api_.create(device_,&ci,nullptr,&pool_);enabled_=last_result_==VK_SUCCESS;
    if(!enabled_)pool_=VK_NULL_HANDLE; // Failure output is undefined, never destroy it.
  }
  ~GpuTimestampProfiler(){if(pool_ && !occupied())api_.destroy(device_,pool_,nullptr);}
  GpuTimestampProfiler(const GpuTimestampProfiler&)=delete;
  GpuTimestampProfiler& operator=(const GpuTimestampProfiler&)=delete;
  // Explicit teardown only after queue stop. Failed submissions retain ranges,
  // even when a later retry might execute their already-recorded reset/writes.
  bool DestroyAfterDeviceStopped(bool stopped) {
    if(!stopped)return false;
    if(pool_)api_.destroy(device_,pool_,nullptr);
    pool_=VK_NULL_HANDLE;enabled_=false;active_=kSlots;records_={};return true;
  }
  bool enabled()const{return enabled_;}
  VkQueryPool pool()const{return pool_;}
  VkResult last_result()const{return last_result_;}
  uint32_t valid_bits()const{return bits_;}
  double period()const{return period_;}
  const Stats& stats()const{return stats_;}
  size_t occupied()const{size_t n=0;for(const auto& r:records_)n+=r.phase!=Phase::Free;return n;}
  static uint64_t Delta(uint64_t start,uint64_t end,uint32_t bits) {
    if(!bits || bits>64)return 0;
    return (end-start)&(bits==64?UINT64_MAX:((uint64_t(1)<<bits)-1));
  }
  // reset(pool,first,count) must record vkCmdResetQueryPool outside any render
  // pass in a freshly opened submission. No host reset extension is assumed.
  template<class Reset> bool Begin(uint64_t ticket,double age,Reset&& reset) {
    if(!enabled_ || !std::isfinite(age) || age<120 || age>=180 || stats_.selected>=kMaxSamples || age<next_age_)return false;
    next_age_=age+0.05;++stats_.eligible;
    if(active_!=kSlots){++stats_.capacity_dropped;return false;}
    for(uint32_t i=0;i<kSlots;++i) {
      auto& r=records_[i];if(r.phase!=Phase::Free)continue;
      r={};r.phase=Phase::Recording;r.ticket=ticket;r.age=age;active_=i;++stats_.selected;
      try{reset(pool_,i*kQueriesPerSlot,kQueriesPerSlot);}catch(...){Fail(ticket);enabled_=false;++stats_.errors;}
      return enabled_;
    }
    ++stats_.capacity_dropped;return false;
  }
  // write(stage,pool,index); ticket guard prevents provider-triggered submission
  // closure from writing an end into a different command stream.
  template<class Write> void Mark(uint64_t ticket,uint32_t category,Write&& write,uint64_t copy_bytes=0) {
    if(!enabled_ || active_==kSlots)return;
    auto& r=records_[active_];if(r.ticket!=ticket || r.phase!=Phase::Recording){Fail(r.ticket);return;}
    if(r.open && r.spans[r.regions-1].category==category){++r.spans[r.regions-1].commands;r.spans[r.regions-1].copy_bytes+=copy_bytes;return;}
    try {
      CloseRegion(r,write);
      if(r.regions==kRegions){if(!r.truncated){r.truncated=true;++stats_.truncated;}return;}
      // An end index is reserved together with each begin, including last pair.
      const uint32_t query=active_*kQueriesPerSlot+2*r.regions;
      r.spans[r.regions++]={category,1,copy_bytes};r.open=true;
      write(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,pool_,query);
    }catch(...){Fail(ticket);enabled_=false;++stats_.errors;}
  }
  template<class Write> void Seal(uint64_t ticket,Write&& write) {
    if(active_==kSlots)return;
    auto& r=records_[active_];if(r.ticket!=ticket){Fail(r.ticket);return;}
    try{CloseRegion(r,write);r.phase=Phase::Recorded;active_=kSlots;}
    catch(...){Fail(ticket);enabled_=false;++stats_.errors;}
  }
  void Submitted(uint64_t ticket) {
    for(auto& r:records_)if(r.ticket==ticket && r.phase==Phase::Recorded){r.phase=Phase::Submitted;++stats_.submitted;}
  }
  void Fail(uint64_t ticket) {
    for(uint32_t i=0;i<kSlots;++i){auto& r=records_[i];
      if(r.ticket==ticket && (r.phase==Phase::Recording || r.phase==Phase::Recorded)){
        r.phase=Phase::Failed;++stats_.failed;if(active_==i)active_=kSlots;
      }
    }
  }
  // Only a same-ticket completion proof authorizes reading/reuse. Even with
  // availability bits, pre-reset data from a prior use would otherwise be stale.
  // emit(record,ns[]) is called only for a complete, unambiguous record.
  template<class Emit> VkResult Collect(uint64_t completed,double age,Emit&& emit) {
    if(!enabled_)return last_result_;
    for(auto& r:records_) {
      if(r.phase!=Phase::Submitted || r.ticket>completed)continue;
      if(!r.regions){++stats_.completed;++stats_.discarded;r={};continue;}
      std::array<Value,kQueriesPerSlot> values{};
      const uint32_t first=uint32_t(&r-records_.data())*kQueriesPerSlot;
      last_result_=api_.get(device_,pool_,first,2*r.regions,sizeof(Value)*2*r.regions,
          values.data(),sizeof(Value),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
      if(last_result_!=VK_SUCCESS && last_result_!=VK_NOT_READY){++stats_.errors;enabled_=false;return last_result_;}
      bool available=last_result_==VK_SUCCESS;
      for(uint32_t i=0;i<2*r.regions;++i)available&=values[i].available!=0;
      if(!available){++stats_.not_ready;if(++r.attempts>=3){++stats_.completed;++stats_.discarded;r={};}continue;}
      // More than one wrap cannot be reconstructed. CPU residence is a safe
      // upper bound for the sampled GPU interval, not a substitute GPU timing.
      const double residence_ns=(age-r.age)*1e9;
      const double wrap_ns=std::ldexp(period_,int(bits_));
      bool ambiguous=!std::isfinite(residence_ns)||residence_ns<0||residence_ns>=wrap_ns;
      std::array<double,kRegions> ns{};
      for(uint32_t i=0;i<r.regions;++i){
        ns[i]=double(Delta(values[2*i].ticks,values[2*i+1].ticks,bits_))*period_;
        ambiguous|=ns[i]>residence_ns+period_;
      }
      ++stats_.completed;
      if(ambiguous){++stats_.ambiguous;++stats_.discarded;}
      else {emit(r,ns);++stats_.emitted;}
      r={};
    }
    return VK_SUCCESS;
  }
 private:
  template<class Write> void CloseRegion(Record& r,Write& write) {
    if(!r.open)return;
    write(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,pool_,active_*kQueriesPerSlot+2*r.regions-1);r.open=false;
  }
  Api api_;VkDevice device_;VkQueryPool pool_=VK_NULL_HANDLE;
  uint32_t bits_;double period_;bool enabled_=false;VkResult last_result_=VK_SUCCESS;
  std::array<Record,kSlots> records_{};uint32_t active_=kSlots;double next_age_=-1;Stats stats_{};
};
class GpuTimestampSubmissionScope {
 public:
  GpuTimestampSubmissionScope(GpuTimestampProfiler* p,uint64_t ticket):p_(p),ticket_(ticket){}
  ~GpuTimestampSubmissionScope(){if(p_&&!submitted_)p_->Fail(ticket_);}
  void Submitted(){if(p_)p_->Submitted(ticket_);submitted_=true;}
 private:GpuTimestampProfiler* p_;uint64_t ticket_;bool submitted_=false;
};
} // namespace acv
