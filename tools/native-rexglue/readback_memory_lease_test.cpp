#include "readback_memory_lease.h"
#include "deferred_readback_queue.h"
#include <array>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <thread>
using Q = acv::DeferredReadbackQueue;
int main() {
  std::recursive_mutex memory_lock;
  std::array<unsigned char,16> bytes{};
  acv::ReadbackMemoryLease lease(memory_lock);
  auto old = lease.GuardPublication([&] { bytes.fill(7); return true; });
  assert(lease.Mutate([&] { bytes[3] = 9; }));
  assert(!old() && bytes[3] == 9 && bytes[0] == 0);
  auto current = lease.GuardPublication([&] { bytes.fill(8); return true; });
  assert(current() && bytes[0] == 8);
  lease.Retire(); assert(!current());
  assert(!lease.Mutate([&] { bytes.fill(0); }));
  // New allocation at the same address must never revive old publication.
  acv::ReadbackMemoryLease replacement(memory_lock);
  assert(replacement.Mutate([&] { bytes.fill(0x55); }));
  assert(!old() && !current() && bytes[0] == 0x55);
  auto fresh = replacement.GuardPublication([&] { bytes.fill(0x66); return true; });
  assert(fresh() && bytes[0] == 0x66);
  std::function<bool()> expired;
  {
    acv::ReadbackMemoryLease temporary(memory_lock);
    expired = temporary.GuardPublication([&] { bytes.fill(0xEE); return true; });
  }
  assert(!expired() && bytes[0] == 0x66);
  // Actual queue wait releases execution to another CPU thread. Mutation during
  // that wait invalidates the captured generation before publication resumes.
  std::mutex rendezvous;
  std::condition_variable cv;
  bool waiting = false, mutated = false;
  acv::ReadbackMemoryLease concurrent(memory_lock);
  Q queue([&](uint64_t) {
    std::unique_lock<std::mutex> lock(rendezvous);
    waiting = true; cv.notify_all();
    cv.wait(lock,[&] { return mutated; });
    return true;
  });
  int publishes = 0;
  assert(queue.Enqueue({{0,16},1,1,concurrent.GuardPublication([&] {
    ++publishes; bytes.fill(1); return true;
  })}));
  std::thread cpu([&] {
    { std::unique_lock<std::mutex> lock(rendezvous); cv.wait(lock,[&] { return waiting; }); }
    assert(concurrent.Mutate([&] { bytes[4] = 0xAB; }));
    { std::lock_guard<std::mutex> lock(rendezvous); mutated = true; }
    cv.notify_all();
  });
  assert(!queue.Drain()); cpu.join();
  assert(queue.failed() && publishes == 0 && bytes[4] == 0xAB);
  assert(queue.CancelAfterDeviceStopped());
  // Correct caller ordering: complete pending bytes, then perform partial write.
  acv::ReadbackMemoryLease ordered(memory_lock);
  Q good([](uint64_t) { return true; });
  assert(good.Enqueue({{0,16},2,2,ordered.GuardPublication([&] {
    bytes.fill(0x22); return true;
  })}));
  assert(good.BeforeCpuAccess({3,1}));
  assert(ordered.Mutate([&] { bytes[3] = 0x99; }));
  assert(good.Drain() && bytes[3] == 0x99 && bytes[2] == 0x22 && bytes[4] == 0x22);
  std::cout << "PASS: generations, address reuse, destruction, concurrent write during wait, ordered partial write\n";
}
