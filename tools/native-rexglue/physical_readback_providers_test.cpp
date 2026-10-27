#include "physical_readback_providers.h"
#include "readback_memory_lease.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <thread>
using Registry=acv::PhysicalReadbackProviders;
int main() {
  std::recursive_mutex memory_mutex;
  Registry providers(memory_mutex);
  acv::ReadbackMemoryLease allocation(memory_mutex);
  std::mutex gate_mutex; std::condition_variable cv;
  bool entered=false, proceed=false;
  int bytes=0, publications=0;
  assert(providers.Register({{0x1000,16},[&] {
    std::unique_lock<std::mutex> gate(gate_mutex);
    entered=true;cv.notify_all();cv.wait(gate,[&] {return proceed;});return true;
  },allocation.GuardPublication([&] { bytes=1;++publications;return true; })}));
  std::thread cpu([&] {
    auto lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
    assert(providers.Synchronize(lock,{0x1000,4}) && lock.owns_lock());
    assert(bytes==2);
  });
  {
    std::unique_lock<std::mutex> gate(gate_mutex);cv.wait(gate,[&] {return entered;});
  }
  // This registration requires the memory lock. It would deadlock if the CPU
  // retained that lock while waiting on the GPU future.
  assert(providers.Register({{0x1000,16},[]{return true;},
      allocation.GuardPublication([&] {bytes=2;++publications;return true;})}));
  {std::lock_guard<std::mutex> gate(gate_mutex);proceed=true;}cv.notify_all();
  cpu.join();assert(publications==2 && providers.pending()==0);
  // Two CPU faults can await one provider; publication must happen exactly once.
  int awaiters=0;proceed=false;
  assert(providers.Register({{0x1000,16},[&] {
    std::unique_lock<std::mutex> gate(gate_mutex);++awaiters;cv.notify_all();
    cv.wait(gate,[&]{return proceed;});return true;
  },allocation.GuardPublication([&]{++publications;bytes=3;return true;})}));
  auto read=[&] {
    auto lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
    assert(providers.Synchronize(lock,{0x1000,4}) && bytes==3);
  };
  std::thread first(read),second(read);
  {
    std::unique_lock<std::mutex> gate(gate_mutex);cv.wait(gate,[&]{return awaiters==2;});
    proceed=true;
  }cv.notify_all();first.join();second.join();
  assert(publications==3 && providers.pending()==0);
  // Completion success is insufficient if allocation ownership changed meanwhile.
  auto stale=allocation.GuardPublication([&]{bytes=0xBAD;++publications;return true;});
  allocation.Retire();
  acv::ReadbackMemoryLease reused(memory_mutex);
  assert(reused.Mutate([&]{bytes=0x55;}));
  assert(providers.Register({{0x1000,16},[]{return true;},stale}));
  auto lock=std::unique_lock<std::recursive_mutex>(memory_mutex);
  assert(!providers.Synchronize(lock,{0x1000,4}) && lock.owns_lock());
  assert(bytes==0x55 && publications==3);
  assert(providers.CancelAfterDeviceStopped());
  lock.unlock();
  Registry failed(memory_mutex);
  assert(failed.Register({{0,4},[]{return false;},[&]{++publications;return true;}}));
  lock.lock();assert(!failed.Synchronize(lock,{0,4}) && lock.owns_lock());lock.unlock();
  assert(publications==3 && failed.CancelAfterDeviceStopped());
  Registry unrelated(memory_mutex);
  assert(unrelated.Register({{0,4},[]{assert(false);return false;},[]{return true;}}));
  lock.lock();assert(unrelated.Synchronize(lock,{8,4}));lock.unlock();
  assert(unrelated.pending()==1 && unrelated.CancelAfterDeviceStopped());
  std::cout<<"PASS: unlocked wait, new generation during wait, concurrent CPU faults, single publication, stale allocation, failed completion\n";
}
