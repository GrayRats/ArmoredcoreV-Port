// Private bounded experiment. Requires an explicit physical resource lease.
// CPU payload changes fail closed. Allocation ABA and other CPU consumers are
// not covered: never enable this in the normal plugin or claim global coherency.
#pragma once
#include "../bake_chain_trace.h"
#include <rex/graphics/command_processor.h>
#include <rex/thread.h>
#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace {
struct AcvDeferredJob {
  VkBuffer slot;
  uint32_t address,bytes;
  uint64_t ticket;
  uint8_t* destination;
  std::vector<uint8_t> baseline;
  std::function<bool()> await;
  std::function<bool()> publish;
};
struct AcvDeferredState {
  std::atomic<rex::graphics::CommandProcessor*> processor{nullptr};
  std::vector<AcvDeferredJob> jobs; // GPU worker only
  bool flushing=false,failed=false;
  AcvBakeChainTrace trace{"deferred"};
};
AcvDeferredState& AcvDeferred() {static AcvDeferredState state;return state;}
bool AcvDeferredEligible(uint32_t address,uint32_t bytes) {
  struct Filter {
    uint64_t first=0,end=0;
    Filter() {
      const char* a=std::getenv("ACV_DEFER_RESOLVE_START");
      const char* b=std::getenv("ACV_DEFER_RESOLVE_LENGTH");
      if(!a || !b)return;
      auto parse=[](const char* text,uint64_t& n) {
        char* tail=nullptr;errno=0;n=std::strtoull(text,&tail,0);
        return !errno && tail!=text && !*tail && *text!='-';
      };
      uint64_t size=0;
      if(parse(a,first) && parse(b,size) && size && size<=0x800000 &&
          first<0x20000000 && size<=0x20000000-first)end=first+size;
      else std::fprintf(stderr,"[acv-deferred] Invalid leased test range; disabled\n");
    }
  };
  static const Filter f;
  return bytes && bytes<=0x800000 && address>=f.first && uint64_t(address)+bytes<=f.end;
}
bool AcvFlushDeferred(rex::graphics::CommandProcessor* processor,const char* reason) {
  auto& s=AcvDeferred();
  if(s.flushing || s.processor.load()!=processor || s.jobs.empty())return !s.failed;
  if(s.failed)return false;
  s.flushing=true;
  s.trace.Record("\"event\":\"deferred_flush\",\"reason\":\"%s\",\"jobs\":%u",reason,unsigned(s.jobs.size()));
  // GPU waits happen outside the memory critical region. A recursive flush
  // from EndSubmission cleanup cannot inspect or retire the current jobs.
  bool ok=true;
  for(auto& job:s.jobs) if(!job.await()) {ok=false;break;}
  if(ok) {
    rex::thread::global_critical_region region;
    auto lock=region.Acquire();
    // Check all baselines before publishing any overlapping snapshots.
    for(const auto& job:s.jobs)
      if(std::memcmp(job.destination,job.baseline.data(),job.bytes)!=0) {ok=false;break;}
    if(ok)for(auto& job:s.jobs) {
      if(!job.publish()) {ok=false;break;}
      s.trace.Record("\"event\":\"deferred_publish\",\"physical\":%u,\"bytes\":%u,\"ticket\":%llu",
          job.address,job.bytes,(unsigned long long)job.ticket);
    }
  }
  s.failed=!ok;
  if(!ok)std::fprintf(stderr,"[acv-deferred] Wait/cache failure or CPU payload changed; refusing publication\n");
  s.jobs.clear();s.flushing=false;return ok;
}
bool AcvRetireDeferredSlot(rex::graphics::CommandProcessor* processor,VkBuffer slot) {
  auto& s=AcvDeferred();
  for(const auto& job:s.jobs)if(job.slot==slot)return AcvFlushDeferred(processor,"slot_reuse");
  return !s.failed;
}
bool AcvAddDeferred(rex::graphics::CommandProcessor* processor,AcvDeferredJob job) {
  auto& s=AcvDeferred();
  if(!s.trace.enabled()) {
    std::fprintf(stderr,"[acv-deferred] Active unique bake trace required; refusing deferral\n");return false;
  }
  if(s.failed || (s.processor.load() && s.processor.load()!=processor))return false;
  if(s.jobs.size()>=16 && !AcvFlushDeferred(processor,"queue_limit"))return false;
  job.baseline.assign(job.destination,job.destination+job.bytes);
  s.processor.store(processor);
  s.trace.Record("\"event\":\"deferred_enqueue\",\"physical\":%u,\"bytes\":%u,\"ticket\":%llu",
      job.address,job.bytes,(unsigned long long)job.ticket);
  s.jobs.push_back(std::move(job));return true;
}
void AcvResetDeferred(rex::graphics::CommandProcessor* processor) {
  auto& s=AcvDeferred();
  if(s.processor.load()==processor) {s.processor.store(nullptr);s.jobs.clear();}
}
} // namespace

// Explicitly exported only by the private experimental plugin. The source-copy
// shim calls this outside the SDK memory lock before reading the leased range.
extern "C" __attribute__((visibility("default")))
bool AcvResolveReadbackForCpu(uint32_t address,uint32_t bytes) {
  auto& state=AcvDeferred();
  auto* processor=state.processor.load();
  if(!processor || !AcvDeferredEligible(address,bytes))return true;
  auto promise=std::make_shared<std::promise<bool>>();auto ready=promise->get_future();
  processor->CallInThreadForReadback([processor,promise]() {
    if(AcvDeferred().processor.load()!=processor) {promise->set_value(false);return;}
    promise->set_value(AcvFlushDeferred(processor,"cpu_consumer"));
  });
  // Dedicated readback requests are serviced even in a guest WAIT packet.
  // Requests retain their state if a driver call stalls the worker.
  // Fail closed on a bounded timeout rather than copying an unready buffer.
  if(ready.wait_for(std::chrono::seconds(5))!=std::future_status::ready) {
    std::fprintf(stderr,"[acv-deferred] GPU worker request timed out\n");return false;
  }
  return ready.get();
}
