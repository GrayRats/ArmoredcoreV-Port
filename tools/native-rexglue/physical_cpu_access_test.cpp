#include "physical_readback_providers.h"
#include <cassert>
#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>
using Registry=acv::PhysicalReadbackProviders;
int main() {
  std::recursive_mutex memory_mutex;
  Registry registry(memory_mutex);
  auto lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
  int bytes=0, publications=0;
  assert(registry.RegisterLocked(lock,{{0x1000,16},[]{return true;},[&]{bytes=1;++publications;return true;}}));
  const auto cpu=registry.BeginCpuAccess(lock,{0x1000,4});
  assert(cpu && bytes==1 && publications==1);
  lock.unlock();
  std::promise<void> registering,register_done,syncing,sync_done;
  auto registered=register_done.get_future();auto synchronized=sync_done.get_future();
  std::thread gpu([&] {
    auto worker_lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
    registering.set_value();
    assert(registry.RegisterLocked(worker_lock,{{0x1000,16},[]{return true;},[&]{
      assert(bytes==2);bytes=3;++publications;return true;
    }}));
    register_done.set_value();
  });
  std::thread allocator([&] {
    auto allocator_lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
    syncing.set_value();assert(registry.Synchronize(allocator_lock,{0x1000,4}));
    sync_done.set_value();
  });
  registering.get_future().wait();syncing.get_future().wait();
  assert(registered.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  assert(synchronized.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  // Both waiters must release the shared mutex, allowing host IO to complete.
  lock.lock();bytes=2;
  assert(!registry.CancelAfterDeviceStopped()); // Live host accesses cannot be discarded.
  assert(!registry.EndCpuAccess(lock,cpu+1));
  assert(registry.EndCpuAccess(lock,cpu));
  // File invalidation/notification belongs here, before releasing this lock.
  lock.unlock();gpu.join();allocator.join();
  lock.lock();assert(registry.Synchronize(lock,{0x1000,4}));
  assert(bytes==3 && publications==2);
  // Disjoint host ranges can progress independently, overlapping ranges serialize.
  const auto first=registry.BeginCpuAccess(lock,{0x1000,4});
  const auto disjoint=registry.BeginCpuAccess(lock,{0x2000,4});
  assert(first && disjoint && first!=disjoint);
  assert(registry.EndCpuAccess(lock,first) && registry.EndCpuAccess(lock,disjoint));
  assert(registry.CancelAfterDeviceStopped());
  assert(!registry.BeginCpuAccess(lock,{0x1000,4}));
  lock.unlock();
  std::cout<<"PASS: pre-IO GPU drain, overlapping registration/allocator waits, unlocked IO, disjoint ranges, exact ownership, teardown rejects live access\n";
}
