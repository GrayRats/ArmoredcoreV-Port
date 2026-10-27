#pragma once
// GPU-worker-owned scheduling core; experimental runtime integration is opt-in.
// Integration MUST intercept all CPU read/write aliases and drain before free,
// protection changes or host access. Never await while holding the memory lock.
// BeforeSlotReuse MUST run before recording a copy into a staging slot.
// Enqueue only submitted copies; retain staging ownership in Job::publish.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <iterator>
#include <thread>
#include <utility>

namespace acv {
class DeferredReadbackQueue {
 public:
  struct Range {
    uint32_t address, bytes;
    bool valid() const {
      return bytes && uint64_t(address) + bytes <= 0x20000000ULL;
    }
    bool overlaps(Range other) const {
      return uint64_t(address) < uint64_t(other.address) + other.bytes &&
             uint64_t(other.address) < uint64_t(address) + bytes;
    }
  };
  struct Job {
    Range range;
    uint64_t slot, ticket;
    // Invalidate mapped GPU memory, then publish exact bytes. False on any failure.
    // The callback owns the staging allocation until publication/cancellation.
    std::function<bool()> publish;
  };
  using Await = std::function<bool(uint64_t)>;
  explicit DeferredReadbackQueue(Await await, size_t max_jobs = 64,
                                 uint64_t max_bytes = 64ULL << 20)
      : await_(std::move(await)), owner_(std::this_thread::get_id()),
        max_jobs_(max_jobs), max_bytes_(max_bytes) {}
  DeferredReadbackQueue(const DeferredReadbackQueue&) = delete;
  DeferredReadbackQueue& operator=(const DeferredReadbackQueue&) = delete;

  // Reserve capacity before taking the memory lock or recording a GPU copy.
  bool PrepareForEnqueue(uint32_t bytes) {
    if(!Enter()) return false;
    Guard guard(*this);
    if(!bytes || !await_ || !max_jobs_ || bytes>max_bytes_) return false;
    return MakeRoom(bytes);
  }
  bool Enqueue(Job job) {
    if(!Enter()) return false;
    Guard guard(*this);
    if(!job.range.valid() || !job.slot || !job.ticket || !job.publish ||
       !await_ || !max_jobs_ || job.range.bytes > max_bytes_ ||
       job.ticket < last_ticket_) return false;
    // Enqueue is too late to protect old bytes from an already submitted copy.
    // Require the producer to retire the slot BEFORE recording that copy.
    for(const auto& j : jobs_) if(j.slot == job.slot) return false;
    if(!MakeRoom(job.range.bytes)) return false;
    const auto ticket = job.ticket;
    const auto bytes = job.range.bytes;
    try { jobs_.push_back(std::move(job)); }
    catch(...) { failed_ = true; return false; }
    last_ticket_ = ticket;
    bytes_ += bytes;
    return true;
  }
  bool BeforeSlotReuse(uint64_t slot) {
    if(!Enter()) return false;
    Guard guard(*this);
    if(!slot) return false;
    return Flush([&](const Job& j) { return j.slot == slot; });
  }
  // Call before ANY CPU read or write, including host accesses bypassing page faults.
  bool BeforeCpuAccess(Range range) {
    if(!Enter()) return false;
    Guard guard(*this);
    if(!range.valid()) return false;
    return Flush([&](const Job& j) { return j.range.overlaps(range); });
  }
  bool Drain() {
    if(!Enter()) return false;
    Guard guard(*this);
    return Flush([](const Job&) { return true; });
  }
  bool failed() const { return failed_; }
  size_t pending() const { return jobs_.size(); }
  uint64_t pending_bytes() const { return bytes_; }
  // Caller must first stop GPU/CPU users. This releases ownership without publishing.
  bool CancelAfterDeviceStopped() {
    if(std::this_thread::get_id() != owner_ || busy_) return false;
    jobs_.clear(); bytes_ = 0; failed_ = true;
    return true;
  }
 private:
  struct Guard {
    DeferredReadbackQueue& queue;
    explicit Guard(DeferredReadbackQueue& q) : queue(q) { queue.busy_ = true; }
    ~Guard() { queue.busy_ = false; }
  };
  bool Enter() {
    return std::this_thread::get_id() == owner_ && !busy_ && !failed_;
  }
  bool MakeRoom(uint32_t bytes) {
    if(jobs_.size()<max_jobs_ && bytes_<=max_bytes_-bytes) return true;
    // Release the smallest oldest prefix satisfying BOTH caps. Waiting for the
    // newest queued copy here would unnecessarily serialize CPU and GPU work.
    // Slot IDs are unique among pending jobs, even when tickets are shared.
    uint64_t released_bytes=0, boundary_slot=0;
    size_t released_jobs=0;
    for(const auto& job:jobs_) {
      released_bytes+=job.range.bytes;
      ++released_jobs;
      if(jobs_.size()-released_jobs<max_jobs_ &&
         bytes_-released_bytes<=max_bytes_-bytes) {
        boundary_slot=job.slot;break;
      }
    }
    if(!boundary_slot) return false;
    return Flush([boundary_slot](const Job& job){return job.slot==boundary_slot;});
  }
  template<class Select> bool Flush(Select select) {
    // Publishing a newer overlapping job first would allow an older delayed job
    // to overwrite it later. Include all earlier overlapping predecessors.
    // A prefix is conservative but guarantees ordering for partial overlaps too.
    auto end = jobs_.begin();
    uint64_t ticket = 0;
    for(auto it = jobs_.begin(); it != jobs_.end(); ++it) {
      if(select(*it)) { end = std::next(it); ticket = it->ticket; }
    }
    if(!ticket) return true;
    try {
      // Submitted tickets are monotonic on one queue: wait only for the newest.
      if(!await_(ticket)) { failed_ = true; return false; }
      while(jobs_.begin() != end) {
        auto it = jobs_.begin();
        if(!it->publish()) { failed_ = true; return false; }
        bytes_ -= it->range.bytes;
        jobs_.erase(it);
      }
    } catch(...) { failed_ = true; return false; }
    return true;
  }
  Await await_;
  const std::thread::id owner_;
  const size_t max_jobs_;
  const uint64_t max_bytes_;
  std::list<Job> jobs_;
  uint64_t bytes_ = 0, last_ticket_ = 0;
  bool busy_ = false, failed_ = false;
};
} // namespace acv
