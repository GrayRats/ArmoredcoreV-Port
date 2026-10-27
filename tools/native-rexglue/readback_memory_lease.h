#pragma once
// Allocation-generation guard for deferred publication. Host-tested core only.
// All aliases of one allocation MUST share this owner and the memory-system lock.
// Retire before freeing/reusing memory; synchronize pending jobs before a partial
// CPU mutation. Stale publication fails, it does not silently discard valid bytes.
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace acv {
class ReadbackMemoryLease {
  struct State {
    explicit State(std::recursive_mutex& mutex) : mutex(mutex) {}
    std::recursive_mutex& mutex; // memory system must outlive all queued jobs
    uint64_t generation = 1;
    bool active = true;
  };
 public:
  explicit ReadbackMemoryLease(std::recursive_mutex& memory_mutex)
      : state_(std::make_shared<State>(memory_mutex)) {}
  ReadbackMemoryLease(const ReadbackMemoryLease&) = delete;
  ReadbackMemoryLease& operator=(const ReadbackMemoryLease&) = delete;
  ~ReadbackMemoryLease() { Retire(); }

  // Capture while the producer owns the allocation and before releasing its
  // memory lock. GPU completion is awaited separately, outside this callback.
  std::function<bool()> GuardPublication(std::function<bool()> publish) const {
    const auto state = state_;
    std::lock_guard<std::recursive_mutex> lock(state->mutex);
    if(!state->active || !publish) return {};
    const auto generation = state->generation;
    return [state, generation, publish = std::move(publish)] {
      std::lock_guard<std::recursive_mutex> lock(state->mutex);
      if(!state->active || state->generation != generation) return false;
      return publish();
    };
  }

  // The actual CPU write executes inside the same lock as generation validation.
  // A check followed by an unlocked write would leave a publication race.
  bool Mutate(const std::function<void()>& write) {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    if(!state_->active || !write) return false;
    if(state_->generation == std::numeric_limits<uint64_t>::max()) {
      state_->active = false;
      return false;
    }
    ++state_->generation;
    write();
    return true;
  }
  // Irreversible for this owner. A newly allocated object at the same physical
  // address receives a distinct State, so old tokens cannot become valid again.
  void Retire() {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    state_->active = false;
  }
 private:
  const std::shared_ptr<State> state_;
};
} // namespace acv
