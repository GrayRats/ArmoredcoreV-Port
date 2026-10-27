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
  struct State {
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
    return IsOwner() && queue_.PrepareForEnqueue(bytes) && memory_.CollectPublishedPhysicalReadbacks();
  }
  bool RetireSlot(uint64_t slot) {
    return IsOwner() && queue_.BeforeSlotReuse(slot) && memory_.CollectPublishedPhysicalReadbacks();
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
  // The transfer and HOST_READ barrier must already be successfully submitted.
  bool Commit(const StatePtr& state,uint64_t ticket,std::function<bool()> publish) {
    if(!IsOwner() || closed_ || !state || state->owner!=this || !state->registered.load() || state->queued.load() || !ticket || !publish || state->failed.load()) return false;
    const bool ok=queue_.Enqueue({state->range,state->slot,ticket,[this,state,publish=std::move(publish)] {
      if(state->ready()) return true;
      if(state->failed.load(std::memory_order_acquire)) return false;
      std::lock_guard<std::recursive_mutex> lock(memory_mutex_);
      bool copied=false;
      try {copied=publish();} catch(...) {}
      if(!copied) {state->failed.store(true,std::memory_order_release);return false;}
      state->published.store(true,std::memory_order_release);return true;
    }});
    state->queued.store(ok,std::memory_order_release);
    if(!ok) state->failed.store(true,std::memory_order_release);
    return ok;
  }
  void Fail(const StatePtr& state) {
    if(state) state->failed.store(true,std::memory_order_release);
  }
  bool Drain() {
    return IsOwner() && queue_.Drain() && memory_.CollectPublishedPhysicalReadbacks();
  }
  // Normal shutdown drains first. Closing wakes all CPU awaiters, even if owner
  // callbacks will never run. A queued callback holds no raw owner after close.
  void Close() {
    std::lock_guard<std::recursive_mutex> lock(control_->mutex);
    if(closed_) return;
    if(!IsOwner()) std::terminate();
    closed_=true;control_->bridge=nullptr;control_->dispatch={};
    for(const auto& weak:states_) if(auto state=weak.lock())
      if(!state->published.load(std::memory_order_acquire)) state->failed.store(true,std::memory_order_release);
    for(const auto& weak:control_->requests) if(auto request=weak.lock()) request->Complete(request->state->ready());
    control_->requests.clear();
  }
  bool CancelAfterDeviceStopped() {
    if(!IsOwner()) return false;
    Close();return queue_.CancelAfterDeviceStopped();
  }
  // GPU-owner-only diagnostic classification; never changes queue policy.
  bool servicing_cpu_access() const {return cpu_access_;}
  size_t pending() const {return queue_.pending();}
  uint64_t pending_bytes() const {return queue_.pending_bytes();}
 private:
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
    return queue_.BeforeCpuAccess(range) && memory_.CollectPublishedPhysicalReadbacks();
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
            if(control->bridge && request->state->queued.load(std::memory_order_acquire) &&
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
  bool closed_=false;
  bool cpu_access_=false;
};
} // namespace acv
