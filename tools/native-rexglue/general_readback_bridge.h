#pragma once
// Owner-thread GPU queue + memory-provider bridge. GPU allocations belong to the
// caller's pool; drain/cancel this bridge before destroying that pool/device.
#include "deferred_readback_queue.h"
#include <atomic>
#include <chrono>
#include <future>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>
namespace acv {
template<class Memory> class GeneralReadbackBridge {
 public:
  using Range=DeferredReadbackQueue::Range;
  using Dispatch=std::function<void(std::function<void()>)>;
  enum class Phase { Preparing, Recorded, Submitted, Published, Failed };
  enum class FlushReason { Capacity, Slot, Cpu, Drain, Limit, Count };
  struct State {
    std::atomic<Phase> phase{Phase::Preparing};
    uint64_t recording_ticket=0;
    Range range;
    uint64_t slot;
    const void* owner=nullptr;
    std::atomic<bool> registered{false};
    std::atomic<bool> published{false},failed{false},queued{false};
    bool ready() const {return published.load(std::memory_order_acquire) && !failed.load(std::memory_order_acquire);}
  };
  using StatePtr=std::shared_ptr<State>;
  GeneralReadbackBridge(Memory& memory,std::recursive_mutex& memory_mutex,Dispatch dispatch,
                        DeferredReadbackQueue::Await await,size_t max_jobs=64,uint64_t max_bytes=64ULL<<20)
      : memory_(memory),memory_mutex_(memory_mutex),queue_(std::move(await),max_jobs,max_bytes),
        owner_(std::this_thread::get_id()),control_(std::make_shared<Control>()) {
    control_->bridge=this;control_->dispatch=std::move(dispatch);
  }
  ~GeneralReadbackBridge() {Close();}
  GeneralReadbackBridge(const GeneralReadbackBridge&)=delete;
  GeneralReadbackBridge& operator=(const GeneralReadbackBridge&)=delete;
  bool Prepare(uint32_t bytes) {
    if(!IsOwner() || closed_ || batch_failed_) return false;
    if(!recorded_.empty() && (recorded_.size()>=kBatchJobs ||
        bytes>kBatchBytes || recorded_bytes_>kBatchBytes-bytes ||
        !queue_.CanAdmit(recorded_.size()+1,recorded_bytes_+bytes))) {
      if(!FlushRecorded(FlushReason::Capacity)) return false;
    }
    return (recorded_.empty() ? queue_.PrepareForEnqueue(bytes) :
            queue_.CanAdmit(recorded_.size()+1,recorded_bytes_+bytes)) &&
        memory_.CollectPublishedPhysicalReadbacks();
  }
  bool RetireSlot(uint64_t slot) {
    if(!IsOwner() || closed_ || batch_failed_) return false;
    for(const auto& job:recorded_) if(job.slot==slot) {
      if(!FlushRecorded(FlushReason::Slot)) return false;
      break;
    }
    return queue_.BeforeSlotReuse(slot) && memory_.CollectPublishedPhysicalReadbacks();
  }
  StatePtr MakeState(Range range,uint64_t slot) {
    if(!IsOwner() || !range.valid() || !slot || closed_) return {};
    auto state=std::make_shared<State>();state->range=range;state->slot=slot;state->owner=this;
    if(states_.size()>=128) states_.erase(std::remove_if(states_.begin(),states_.end(),
        [](const auto& weak){return weak.expired();}),states_.end());
    states_.push_back(state);return state;
  }
  bool RegisterLocked(std::unique_lock<std::recursive_mutex>& lock,const StatePtr& state) {
    if(!IsOwner() || closed_ || !state || state->owner!=this || state->registered.load() || state->failed.load() || state->queued.load()) return false;
    auto control=control_;
    typename Memory::PhysicalReadbackProvider provider{
      {state->range.address,state->range.bytes},
      [control,state]{return Await(control,state);},
      [state]{return state->ready();},
      [state]{return state->ready();}
    };
    if(!memory_.RegisterExplicitPhysicalReadbackLocked(lock,std::move(provider))) {
      state->failed.store(true,std::memory_order_release);return false;
    }
    state->registered.store(true,std::memory_order_release);return true;
  }
  static constexpr size_t kBatchJobs=4;
  static constexpr uint64_t kBatchBytes=16ULL<<20;
  using Submit=std::function<bool(uint64_t)>;
  void EnableBatching(Submit submit) {
    if(!IsOwner() || !states_.empty() || !submit) std::terminate();
    submit_=std::move(submit);
  }
  bool batching() const {return bool(submit_);}
  size_t recorded() const {return recorded_.size();}
  uint64_t recorded_bytes() const {return recorded_bytes_;}
  struct Statistics {
    uint64_t submissions=0,jobs=0,max_jobs=0;
    uint64_t flushes[size_t(FlushReason::Count)]={};
  };
  const Statistics& statistics() const {return statistics_;}
  void EnableStatistics() {statistics_enabled_=true;}
  // Called only after copy + HOST_READ barrier recording and memory unlock.
  bool Record(const StatePtr& state,uint64_t ticket,std::function<bool()> publish) {
    if(!IsOwner() || closed_ || batch_failed_ || !submit_ || !state ||
       state->owner!=this || !state->registered.load() || state->failed.load() ||
       state->phase.load()!=Phase::Preparing || !ticket || !publish ||
       (!recorded_.empty() && (recorded_.front().ticket!=ticket ||
         state->range.bytes>kBatchBytes || recorded_bytes_>kBatchBytes-state->range.bytes)) ||
       !queue_.CanAdmit(recorded_.size()+1,recorded_bytes_+state->range.bytes)) return false;
    try {recorded_.push_back({state->range,state->slot,ticket,Publisher(state,std::move(publish))});}
    catch(...) {Fail(state);batch_failed_=true;return false;}
    state->recording_ticket=ticket;
    state->phase.store(Phase::Recorded,std::memory_order_release);
    recorded_bytes_+=state->range.bytes;
    if(recorded_.size()>=kBatchJobs || recorded_bytes_>=kBatchBytes)
      return FlushRecorded(FlushReason::Limit);
    return true;
  }
  bool FlushRecorded(FlushReason reason) {
    if(!IsOwner() || closed_ || batch_failed_) return false;
    if(recorded_.empty()) return true;
    if(flushing_) {SubmissionFailed(recorded_.front().ticket);return false;}
    const uint64_t ticket=recorded_.front().ticket;
    flushing_=true;
    if(statistics_enabled_) ++statistics_.flushes[size_t(reason)];
    bool ok=false;
    try {ok=submit_(ticket);} catch(...) {}
    flushing_=false;
    if(!ok || !recorded_.empty()) {SubmissionFailed(ticket);return false;}
    return !batch_failed_;
  }
  // Every actual EndSubmission path settles ONLY recorded jobs, never preparing
  // providers. Preallocated no-wait append cannot recursively enter EndSubmission.
  bool SubmissionSucceeded(uint64_t ticket) {
    if(recorded_.empty()) return !batch_failed_;
    if(!IsOwner() || closed_ || batch_failed_ || recorded_.front().ticket!=ticket ||
       !queue_.AppendSubmittedNoWait(recorded_)) {
      SubmissionFailed(ticket);return false;
    }
    size_t count=0;
    for(const auto& weak:states_) if(auto state=weak.lock()) {
      if(state->recording_ticket==ticket && state->phase.load()==Phase::Recorded) {
        state->queued.store(true,std::memory_order_release);
        state->phase.store(Phase::Submitted,std::memory_order_release);++count;
      }
    }
    recorded_bytes_=0;
    if(statistics_enabled_) {
      ++statistics_.submissions;statistics_.jobs+=count;
      statistics_.max_jobs=std::max<uint64_t>(statistics_.max_jobs,count);
    }
    return true;
  }
  void SubmissionFailed(uint64_t ticket) {
    if(recorded_.empty()) return;
    // A wrong-ticket completion is an invariant failure, not permission to keep
    // accepting work. Preserve every recorded lease, exclude preparing states.
    (void)ticket;
    batch_failed_=true;
    for(const auto& weak:states_) if(auto state=weak.lock())
      if(state->phase.load()==Phase::Recorded) Fail(state);
  }
  // The transfer and HOST_READ barrier must already be successfully submitted.
  bool Commit(const StatePtr& state,uint64_t ticket,std::function<bool()> publish) {
    if(!IsOwner() || closed_ || !state || state->owner!=this || !state->registered.load() || state->queued.load() || !ticket || !publish || state->failed.load()) return false;
    const bool ok=queue_.Enqueue({state->range,state->slot,ticket,Publisher(state,std::move(publish))});
    state->phase.store(ok ? Phase::Submitted : Phase::Failed,std::memory_order_release);
    state->queued.store(ok,std::memory_order_release);
    if(!ok) state->failed.store(true,std::memory_order_release);
    return ok;
  }
  void Fail(const StatePtr& state) {
    if(state) {state->failed.store(true,std::memory_order_release);state->phase.store(Phase::Failed,std::memory_order_release);}
  }
  bool Drain() {
    return IsOwner() && FlushRecorded(FlushReason::Drain) && queue_.Drain() && memory_.CollectPublishedPhysicalReadbacks();
  }
  // Normal shutdown drains first. Closing wakes all CPU awaiters, even if owner
  // callbacks will never run. A queued callback holds no raw owner after close.
  void Close() {
    std::lock_guard<std::recursive_mutex> lock(control_->mutex);
    if(closed_) return;
    if(!IsOwner()) std::terminate();
    closed_=true;control_->bridge=nullptr;control_->dispatch={};
    for(const auto& weak:states_) if(auto state=weak.lock())
      if(!state->published.load(std::memory_order_acquire)) Fail(state);
    for(const auto& weak:control_->requests) if(auto request=weak.lock()) request->Complete(request->state->ready());
    control_->requests.clear();
  }
  bool CancelAfterDeviceStopped() {
    if(!IsOwner()) return false;
    Close();
    if(!queue_.CancelAfterDeviceStopped()) return false;
    recorded_.clear();recorded_bytes_=0;return true;
  }
  // GPU-owner-only diagnostic classification; never changes queue policy.
  bool servicing_cpu_access() const {return cpu_access_;}
  size_t pending() const {return queue_.pending()+recorded_.size();}
  uint64_t pending_bytes() const {return queue_.pending_bytes()+recorded_bytes_;}
 private:
  std::function<bool()> Publisher(StatePtr state,std::function<bool()> publish) {
    return [this,state=std::move(state),publish=std::move(publish)] {
      if(state->ready()) return true;
      if(state->failed.load(std::memory_order_acquire)) return false;
      std::lock_guard<std::recursive_mutex> lock(memory_mutex_);
      bool copied=false;try {copied=publish();} catch(...) {}
      if(!copied) {Fail(state);return false;}
      state->phase.store(Phase::Published,std::memory_order_release);
      state->published.store(true,std::memory_order_release);return true;
    };
  }
  struct Request {
    StatePtr state;
    std::promise<bool> promise;
    std::atomic<bool> finished{false};
    void Complete(bool result) {if(!finished.exchange(true)) promise.set_value(result);}
  };
  struct Control {
    std::recursive_mutex mutex;
    GeneralReadbackBridge* bridge=nullptr;
    Dispatch dispatch;
    std::vector<std::weak_ptr<Request>> requests;
  };
  bool IsOwner() const {return owner_==std::this_thread::get_id();}
  bool ForCpu(Range range) {
    if(!IsOwner()) return false;
    struct Guard {
      bool& flag;bool previous;
      explicit Guard(bool& value):flag(value),previous(value){flag=true;}
      ~Guard(){flag=previous;}
    } guard(cpu_access_);
    for(const auto& job:recorded_) if(job.range.overlaps(range)) {
      if(!FlushRecorded(FlushReason::Cpu)) return false;
      break;
    }
    return !batch_failed_ && queue_.BeforeCpuAccess(range) && memory_.CollectPublishedPhysicalReadbacks();
  }
  static bool Await(const std::shared_ptr<Control>& control,const StatePtr& state) {
    if(state->ready()) return true;
    if(state->failed.load(std::memory_order_acquire)) return false;
    auto request=std::make_shared<Request>();request->state=state;
    auto completion=request->promise.get_future();
    {
      // Recursive for the GPU owner's inline dispatch. CPU memory-provider
      // waits arrive here only after releasing the memory-system mutex.
      std::lock_guard<std::recursive_mutex> lock(control->mutex);
      if(state->ready()) return true;
      if(!control->bridge || !control->dispatch || state->failed.load()) return false;
      control->requests.erase(std::remove_if(control->requests.begin(),control->requests.end(),
          [](const auto& weak){return weak.expired();}),control->requests.end());
      control->requests.push_back(request);
      try {
        control->dispatch([control,request] {
          if(request->finished.load()) return;
          bool ok=request->state->ready();
          if(!ok) {
            std::lock_guard<std::recursive_mutex> owner_lock(control->mutex);
            if(control->bridge && (request->state->queued.load(std::memory_order_acquire) ||
                request->state->phase.load(std::memory_order_acquire)==Phase::Recorded) &&
               !request->state->failed.load(std::memory_order_acquire)) {
              ok=control->bridge->ForCpu(request->state->range) && request->state->ready();
            }
          }
          if(!ok) request->state->failed.store(true,std::memory_order_release);
          request->Complete(ok);
        });
      } catch(...) {
        state->failed.store(true,std::memory_order_release);request->Complete(false);
      }
    }
    if(completion.wait_for(std::chrono::seconds(30))!=std::future_status::ready) {
      state->failed.store(true,std::memory_order_release);request->Complete(false);return false;
    }
    return completion.get();
  }
  Memory& memory_;
  std::recursive_mutex& memory_mutex_;
  DeferredReadbackQueue queue_;
  const std::thread::id owner_;
  std::shared_ptr<Control> control_;
  std::vector<std::weak_ptr<State>> states_;
  Submit submit_;
  std::list<DeferredReadbackQueue::Job> recorded_;
  uint64_t recorded_bytes_=0;
  bool flushing_=false,batch_failed_=false,statistics_enabled_=false;
  Statistics statistics_;
  bool closed_=false;
  bool cpu_access_=false;
};
} // namespace acv
