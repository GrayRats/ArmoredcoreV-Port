#pragma once
// Two-phase physical-memory provider registry. All operations use the same
// memory-system lock; Synchronize requires that lock held exactly once.
// Explicit access boundary only: does not install page-fault interception.
#include <algorithm>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <list>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>
#include <thread>
#include <vector>
namespace acv {
class PhysicalReadbackProviders {
 public:
  struct Range {
    uint32_t address, bytes;
    bool valid() const { return bytes && uint64_t(address)+bytes<=0x20000000ULL; }
    bool overlaps(Range r) const {
      return uint64_t(address)<uint64_t(r.address)+r.bytes &&
             uint64_t(r.address)<uint64_t(address)+bytes;
    }
  };
  struct Provider {
    Range range;
    // Await may run concurrently for the same retained provider; use an
    // idempotent completion future. Publish runs once under the memory lock.
    std::function<bool()> await;
    std::function<bool()> publish;
    // Optional non-blocking proof that an owner queue already published bytes.
    // Allows completed providers to release staging even if no CPU faults occur.
    std::function<bool()> already_published = {};
  };
  explicit PhysicalReadbackProviders(std::recursive_mutex& mutex) : mutex_(mutex) {}
  PhysicalReadbackProviders(const PhysicalReadbackProviders&) = delete;
  PhysicalReadbackProviders& operator=(const PhysicalReadbackProviders&) = delete;
  bool Register(Provider provider) {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    return RegisterLocked(lock, std::move(provider));
  }
  // Borrow an existing memory lock held exactly once. Registration may wait for
  // a host CPU access that already owns this physical range (disk IO/decoder).
  bool RegisterLocked(std::unique_lock<std::recursive_mutex>& lock, Provider provider,
                      const std::function<bool()>& still_owned = {}) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !provider.range.valid() || publishing_ || failed_) return false;
    if(HasOwnedCpuAccess(provider.range)) return false;
    while(!failed_ && HasCpuAccess(provider.range)) access_changed_.wait(lock);
    if(failed_ || publishing_ || !provider.range.valid() || !provider.await || !provider.publish) return false;
    if(providers_.size()>=64 && !CollectPublished(lock)) return false;
    if(providers_.size()>=64) return false;
    if(still_owned) {
      try { if(!still_owned()) return false; } catch(...) { return false; }
    }
    const Range registered = provider.range;
    try { providers_.push_back(std::make_shared<Provider>(std::move(provider))); }
    catch(...) { failed_=true; access_changed_.notify_all(); return false; }
    provider_begin_ = std::min(provider_begin_, registered.address);
    provider_end_ = std::max(provider_end_, registered.address + registered.bytes);
    return true;
  }
  bool CollectPublished(std::unique_lock<std::recursive_mutex>& lock) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || publishing_ || failed_) return false;
    publishing_=true;
    try {
      while(!providers_.empty()) {
        const auto& provider=providers_.front();
        if(!provider->already_published || !provider->already_published()) break;
        if(!provider->publish()) { failed_=true;break; }
        providers_.pop_front();
      }
    } catch(...) { failed_=true; }
    publishing_=false;
    RecomputeProviderBounds();
    if(failed_) access_changed_.notify_all();
    return !failed_;
  }
  // A copy performed entirely under this lock needs no long-lived CPU pin.
  // False means use Synchronize/BeginCpuAccess; never unlock after a true result
  // until the copy completes. Pending adjacent bytes do not block private reads.
  bool CanAccessWithoutWait(const std::unique_lock<std::recursive_mutex>& lock, Range range) const {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !range.valid() || publishing_ || failed_ ||
       HasCpuAccess(range)) return false;
    return !HasProvider(range);
  }
  // Wait for host IO/decoder ownership without forcing older GPU copies to
  // publish. Used before a new GPU write; actual CPU reads still Synchronize.
  bool WaitCpuAccess(std::unique_lock<std::recursive_mutex>& lock, Range range) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !range.valid() || publishing_ || failed_ ||
       HasOwnedCpuAccess(range)) return false;
    while(!failed_ && HasCpuAccess(range)) access_changed_.wait(lock);
    return !failed_;
  }
  bool Synchronize(std::unique_lock<std::recursive_mutex>& lock, Range range,
                   bool allow_owned_cpu_access = false) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !range.valid()) return false;
    if(publishing_ || failed_) return false;
    for (;;) {
      if(!allow_owned_cpu_access && HasOwnedCpuAccess(range)) return false;
      while(!failed_ && HasCpuAccess(range, allow_owned_cpu_access)) access_changed_.wait(lock);
      if(failed_) return false;
      if(!MayHaveProvider(range)) return true;
      auto end=providers_.begin();
      for(auto it=providers_.begin();it!=providers_.end();++it)
        if((*it)->range.overlaps(range)) end=std::next(it);
      if(end==providers_.begin()) return true;
      std::vector<std::shared_ptr<Provider>> pending(providers_.begin(),end);
      bool ready=true;
      lock.unlock();
      try { for(const auto& p:pending) if(!p->await()) { ready=false; break; } }
      catch(...) { ready=false; }
      lock.lock();
      if(!ready) { failed_=true; access_changed_.notify_all(); return false; }
      if(failed_) return false;
      // Another CPU may have completed and removed these providers while this
      // thread was waiting. Retained snapshots must never publish them twice.
      publishing_=true;
      for(const auto& p:pending) {
        auto it=std::find(providers_.begin(),providers_.end(),p);
        if(it==providers_.end()) continue;
        bool ok=false;
        try { ok=p->publish(); } catch(...) {}
        if(!ok) { failed_=true; publishing_=false; access_changed_.notify_all(); return false; }
        providers_.erase(it);
      }
      publishing_=false;
      RecomputeProviderBounds();
      // Registration while unlocked may have added a new overlapping result.
      // Recheck before reporting the CPU range ready.
    }
  }
  // Synchronize before reserving. The caller may then unlock for host IO or
  // decoding: overlapping GPU registration, CPU access and allocator mutation
  // wait until EndCpuAccess, without holding the memory-system mutex.
  uint64_t BeginCpuAccess(std::unique_lock<std::recursive_mutex>& lock, Range range) {
    return BeginCpuAccess(lock, std::vector<Range>{range});
  }
  uint64_t BeginCpuAccess(std::unique_lock<std::recursive_mutex>& lock,
                          const std::vector<Range>& ranges) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || ranges.empty() ||
       ranges.size()>64 || publishing_ || failed_) return 0;
    for(const auto& range:ranges) if(!range.valid()) return 0;
    for(;;) {
      // No partial reservations while waiting on GPU. Another CPU may register
      // new work for an earlier range while a later range wait unlocks us.
      for(const auto& range:ranges) if(!Synchronize(lock,range)) return 0;
      bool retry=false;
      for(const auto& range:ranges) {
        if(HasCpuAccess(range) || HasProvider(range)) {
          retry=true;break;
        }
      }
      if(retry) continue;
      if(cpu_accesses_.size()>=64 || next_access_==UINT64_MAX) return 0;
      const uint64_t token=next_access_++;
      try { cpu_accesses_.push_back({token,ranges,std::this_thread::get_id()}); }
      catch(...) { failed_=true;access_changed_.notify_all();return 0; }
      return token;
    }
  }
  bool OwnsCpuRange(std::unique_lock<std::recursive_mutex>& lock, uint64_t token, Range range) const {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !token || !range.valid()) return false;
    auto it=std::find_if(cpu_accesses_.begin(),cpu_accesses_.end(),
                         [token](const CpuAccess& access){return access.token==token;});
    return it!=cpu_accesses_.end() && it->owner==std::this_thread::get_id() &&
        std::any_of(it->ranges.begin(),it->ranges.end(),[range](Range owned){
          return owned.address<=range.address && uint64_t(owned.address)+owned.bytes>=uint64_t(range.address)+range.bytes;
        });
  }
  bool EndCpuAccess(std::unique_lock<std::recursive_mutex>& lock, uint64_t token) {
    if(!lock.owns_lock() || lock.mutex()!=&mutex_ || !token) return false;
    auto it=std::find_if(cpu_accesses_.begin(),cpu_accesses_.end(),
                         [token](const CpuAccess& access){return access.token==token;});
    if(it==cpu_accesses_.end() || it->owner!=std::this_thread::get_id()) return false;
    cpu_accesses_.erase(it);access_changed_.notify_all();return true;
  }
  bool CancelAfterDeviceStopped() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(publishing_ || !cpu_accesses_.empty()) return false;
    failed_=true; providers_.clear();RecomputeProviderBounds();access_changed_.notify_all();return true;
  }
  size_t pending() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_); return providers_.size();
  }
 private:
  // Conservative rejection only: a range inside the envelope still scans all
  // providers. All mutations and readers own mutex_, including after GPU waits.
  bool MayHaveProvider(Range range) const {
    return range.address < provider_end_ && uint64_t(range.address) + range.bytes > provider_begin_;
  }
  bool HasProvider(Range range) const {
    return MayHaveProvider(range) && std::any_of(providers_.begin(),providers_.end(),
        [range](const auto& provider){return provider->range.overlaps(range);});
  }
  void RecomputeProviderBounds() {
    provider_begin_ = 0x20000000; provider_end_ = 0;
    for(const auto& provider:providers_) {
      provider_begin_ = std::min(provider_begin_, provider->range.address);
      provider_end_ = std::max(provider_end_, provider->range.address + provider->range.bytes);
    }
  }
  struct CpuAccess { uint64_t token; std::vector<Range> ranges; std::thread::id owner; };
  bool HasCpuAccess(Range range, bool ignore_owned=false) const {
    return std::any_of(cpu_accesses_.begin(),cpu_accesses_.end(),
        [range,ignore_owned](const CpuAccess& access){
          if(ignore_owned && access.owner==std::this_thread::get_id()) return false;
          return std::any_of(access.ranges.begin(),access.ranges.end(),
                            [range](Range owned){return owned.overlaps(range);});
        });
  }
  bool HasOwnedCpuAccess(Range range) const {
    return std::any_of(cpu_accesses_.begin(),cpu_accesses_.end(),[range](const CpuAccess& access){
      return access.owner==std::this_thread::get_id() &&
          std::any_of(access.ranges.begin(),access.ranges.end(),
                      [range](Range owned){return owned.overlaps(range);});
    });
  }
  std::vector<CpuAccess> cpu_accesses_;
  std::condition_variable_any access_changed_;
  uint64_t next_access_=1;
  std::recursive_mutex& mutex_;
  std::list<std::shared_ptr<Provider>> providers_;
  bool publishing_=false, failed_=false;
  uint32_t provider_begin_=0x20000000, provider_end_=0;
};
} // namespace acv
