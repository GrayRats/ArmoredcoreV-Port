// Real POSIX access violations across all guest physical aliases. SDK mapping
// uses fixed guest addresses incompatible with ASan shadow; core concurrency and
// generation tests are separately run with ASan/UBSan.
#include <rex/logging.h>
#include <rex/system/xmemory.h>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <thread>
using namespace rex::memory;
int main() {
  rex::InitLogging();
  Memory memory;
  assert(memory.Initialize());
  constexpr uint32_t physical=0x10000;
  constexpr uint32_t aliases[]={0xA0010000,0xC0010000,0xE000F000};
  const auto rw=kMemoryProtectRead|kMemoryProtectWrite;
  assert(memory.LookupHeap(aliases[0])->AllocFixed(aliases[0],0x10000,0x10000,kMemoryAllocationCommit,rw));
  assert(memory.LookupHeap(aliases[2])->AllocFixed(aliases[2],0x1000,0x1000,kMemoryAllocationCommit,rw));
  auto* destination=memory.TranslatePhysical<uint8_t*>(physical);
  std::memset(destination,0,0x1000);
  std::mutex gate_mutex;
  std::condition_variable cv;
  bool waiting=false,complete=false;
  unsigned publications=0;
  assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[&] {
    std::unique_lock gate(gate_mutex);waiting=true;cv.notify_all();
    cv.wait(gate,[&]{return complete;});return true;
  },[&] {std::memset(destination,0x34,0x1000);++publications;return true;}}));
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,false,true);
  std::thread gpu([&] {
    {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return waiting;});}
    // Real fault handler must release the SDK global lock while awaiting us.
    auto lock=rex::thread::global_critical_region::AcquireDirect();
    {std::lock_guard gate(gate_mutex);complete=true;}cv.notify_all();
  });
  for(auto alias:aliases) {
    auto* ptr=memory.TranslateVirtual<volatile uint8_t*>(alias);
    assert(ptr[0]==0x34 && ptr[4095]==0x34);
  }
  gpu.join();assert(publications==1);
  unsigned invalidations=0;
  auto handle=memory.RegisterPhysicalMemoryInvalidationCallback(
      [](void* context,uint32_t,uint32_t,bool) {
        ++*static_cast<unsigned*>(context);return std::pair<uint32_t,uint32_t>{0,UINT32_MAX};
      },&invalidations);
  assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&] {
    std::memset(destination,0x45,0x1000);++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,true,true);
  auto* first=memory.TranslateVirtual<volatile uint8_t*>(aliases[0]);
  first[0]=0x66;
  assert(first[0]==0x66 && first[1]==0x45 && publications==2 && invalidations>0);
  for(auto alias:aliases) {
    auto* ptr=memory.TranslateVirtual<volatile uint8_t*>(alias);
    assert(ptr[0]==0x66 && ptr[1]==0x45);
  }
  memory.UnregisterPhysicalMemoryInvalidationCallback(handle);
  // Protect/commit replace host page protection. Old watch bits must be retired
  // so the next generation can arm a new no-access watch on the same address.
  for(bool fixed_commit:{false,true}) {
    assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&] {
      std::memset(destination,0x56,0x1000);++publications;return true;
    }}));
    memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,false,true);
    auto* owner=memory.LookupHeap(aliases[0]);
    if(fixed_commit) {
      assert(owner->AllocFixed(aliases[0],0x10000,0x10000,kMemoryAllocationCommit,rw));
    } else {
      assert(owner->Protect(aliases[0],0x10000,kMemoryProtectRead));
    }
    assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&] {
      std::memset(destination,0x57,0x1000);++publications;return true;
    }}));
    memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,false,true);
    assert(memory.TranslateVirtual<volatile uint8_t*>(aliases[0])[0]==0x57);
    assert(owner->Protect(aliases[0],0x10000,rw));
  }
  const auto before_reuse=publications;
  // An allocator waiting on a provider must not free a replacement owner.
  constexpr uint32_t reuse_physical=0x02000000;
  constexpr uint32_t reuse_guest=0xE1FFF000;
  auto* heap=memory.LookupHeap(reuse_guest);
  const auto reserve_commit=kMemoryAllocationReserve|kMemoryAllocationCommit;
  assert(heap->AllocFixed(reuse_guest,0x1000,0x1000,reserve_commit,rw));
  bool first_waiting=false,release_first=false;
  unsigned waiters=0;
  assert(memory.RegisterExplicitPhysicalReadback({{reuse_physical,0x1000},[&] {
    std::unique_lock gate(gate_mutex);
    if (++waiters==1) {
      first_waiting=true;cv.notify_all();cv.wait(gate,[&]{return release_first;});
    }
    return true;
  },[&] {
    std::memset(memory.TranslatePhysical<uint8_t*>(reuse_physical),0x67,0x1000);
    ++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(reuse_physical,0x1000,false,true);
  bool old_release_result=true;
  std::thread old_releaser([&]{old_release_result=heap->Release(reuse_guest);});
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return first_waiting;});}
  assert(heap->Release(reuse_guest));
  assert(heap->AllocFixed(reuse_guest,0x1000,0x1000,reserve_commit,rw));
  auto* reused=memory.TranslateVirtual<volatile uint8_t*>(reuse_guest);
  reused[0]=0x79;
  {std::lock_guard gate(gate_mutex);release_first=true;}cv.notify_all();
  old_releaser.join();
  assert(!old_release_result && reused[0]==0x79 && publications==before_reuse+1);
  // Decommit must publish while the old allocation is still committed/mapped.
  assert(memory.RegisterExplicitPhysicalReadback({{reuse_physical,0x1000},[]{return true;},[&] {
    HeapAllocationInfo info{};
    assert(heap->QueryRegionInfo(reuse_guest,&info) && (info.state&kMemoryAllocationCommit));
    std::memset(memory.TranslatePhysical<uint8_t*>(reuse_physical),0x88,0x1000);
    ++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(reuse_physical,0x1000,false,true);
  assert(heap->Decommit(reuse_guest,0x1000));
  assert(publications==before_reuse+2);
  assert(heap->Release(reuse_guest));
  assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  std::cout<<"PASS: actual read/write faults, A/C/E aliases, unlocked GPU wait, one publication, partial CPU write, write-watch coexistence, pre-decommit publication and same-address reuse race\n";
}
