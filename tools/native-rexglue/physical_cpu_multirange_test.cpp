#include "physical_readback_providers.h"
#include <cassert>
#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>
using Registry=acv::PhysicalReadbackProviders;
int main() {
  std::recursive_mutex mutex;Registry registry(mutex);
  std::mutex gate_mutex;std::condition_variable cv;
  bool awaiting=false,complete=false,owned=false,finish=false;
  int data_a=0,data_b=0;
  assert(registry.Register({{0x3000,16},[&] {
    std::unique_lock gate(gate_mutex);awaiting=true;cv.notify_all();
    cv.wait(gate,[&]{return complete;});return true;
  },[&]{data_b=2;return true;}}));
  std::thread decoder([&] {
    auto lock=std::unique_lock<std::recursive_mutex>(mutex);
    const auto token=registry.BeginCpuAccess(lock,std::vector<Registry::Range>{{0x1000,16},{0x3000,16}});
    assert(token && data_a==1 && data_b==2);
    // Context metadata may fault from this CPU while its raw buffers are pinned.
    assert(registry.Synchronize(lock,{0x1000,4},true));
    assert(!registry.Synchronize(lock,{0x1000,4})); // Allocator may not free its own pin.
    assert(!registry.RegisterLocked(lock,{{0x1000,4},[]{return true;},[]{return true;}}));
    lock.unlock();
    {std::unique_lock gate(gate_mutex);owned=true;cv.notify_all();cv.wait(gate,[&]{return finish;});}
    lock.lock();assert(registry.EndCpuAccess(lock,token));
  });
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return awaiting;});}
  // If the decoder reserved A before waiting on B, this producer would block
  // and prevent completion of B: the exact dependency cycle under test.
  assert(registry.Register({{0x1000,16},[]{return true;},[&]{data_a=1;return true;}}));
  {std::lock_guard gate(gate_mutex);complete=true;}cv.notify_all();
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return owned;});}
  std::promise<void> trying,registered;auto registration=registered.get_future();
  std::thread gpu([&] {
    trying.set_value();assert(registry.Register({{0x3000,4},[]{return true;},[]{return true;}}));
    registered.set_value();
  });
  trying.get_future().wait();
  assert(registration.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  {std::lock_guard gate(gate_mutex);finish=true;}cv.notify_all();
  decoder.join();gpu.join();
  auto lock=std::unique_lock<std::recursive_mutex>(mutex);
  assert(registry.Synchronize(lock,{0x3000,4}));
  assert(registry.CancelAfterDeviceStopped());
  std::cout<<"PASS: atomic multi-range pin, no partial reservation during GPU wait, recheck earlier range, owner fault access, overlapping producer waits\n";
}
