// Exercises the compiled SDK Memory bridge; no guest mapping or game required.
#include <rex/system/xmemory.h>
#include "readback_memory_lease.h"
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <thread>
int main() {
  rex::memory::Memory memory;
  auto& mutex=rex::thread::global_critical_region::mutex();
  acv::ReadbackMemoryLease lease(mutex);
  std::mutex gate_mutex;
  std::condition_variable cv;
  bool waiting=false,done=false;
  unsigned char bytes[16]={};
  int publications=0;
  assert(memory.RegisterExplicitPhysicalReadback({{0x1000,16},[&] {
    std::unique_lock gate(gate_mutex);waiting=true;cv.notify_all();
    cv.wait(gate,[&]{return done;});return true;
  },lease.GuardPublication([&] {
    std::fill(std::begin(bytes),std::end(bytes),0x34);++publications;return true;
  })}));
  std::thread cpu([&] {
    auto lock=rex::thread::global_critical_region::AcquireDirect();
    assert(memory.SynchronizeExplicitPhysicalReadback(lock,0x1000,1));
    assert(lock.owns_lock());
    assert(lease.Mutate([&]{bytes[0]=0x55;}));
    assert(bytes[0]==0x55 && bytes[1]==0x34 && publications==1);
  });
  {
    std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return waiting;});
  }
  // Must acquire the actual SDK global lock while the CPU waits on completion.
  {
    auto lock=rex::thread::global_critical_region::AcquireDirect();
    std::lock_guard gate(gate_mutex);done=true;
  }
  cv.notify_all();cpu.join();
  auto lock=rex::thread::global_critical_region::AcquireDirect();
  assert(memory.SynchronizeExplicitPhysicalReadback(lock,0x1000,16));
  assert(!memory.SynchronizeExplicitPhysicalReadback(lock,0x1FFFFFFF,2));
  lock.unlock();
  assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  assert(!memory.RegisterExplicitPhysicalReadback({{0x1000,4},[]{return true;},[]{return true;}}));
  std::cout<<"PASS: compiled SDK Memory, global unlock during completion, ordered publication, partial CPU write, invalid range, stopped device\n";
}
