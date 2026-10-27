#include "physical_readback_providers.h"
#include "deferred_readback_queue.h"
#include <cassert>
#include <iostream>
int main() {
  {
    std::recursive_mutex mutex;acv::PhysicalReadbackProviders providers(mutex);
    std::unique_lock<std::recursive_mutex> lock(mutex);
    assert(providers.CanAccessWithoutWait(lock,{0,4}));
    assert(!providers.CanAccessWithoutWait(lock,{0,0}));
    assert(providers.RegisterLocked(lock,{{128,4},[]{return true;},[]{return true;}}));
    assert(providers.CanAccessWithoutWait(lock,{0,4})); // Adjacent bytes, private mapping.
    assert(!providers.CanAccessWithoutWait(lock,{128,4}));
    assert(providers.Synchronize(lock,{128,4}));
    auto pin=providers.BeginCpuAccess(lock,{0,4096});assert(pin);
    assert(!providers.CanAccessWithoutWait(lock,{128,4})); // Same-page CPU reservation.
    assert(providers.EndCpuAccess(lock,pin));
    assert(providers.CanAccessWithoutWait(lock,{128,4}));
    lock.unlock();assert(!providers.CanAccessWithoutWait(lock,{128,4}));lock.lock();
    assert(providers.CancelAfterDeviceStopped());
    assert(!providers.CanAccessWithoutWait(lock,{128,4})); // Failed/stopped is not a ready range.
  }

  std::recursive_mutex mutex;
  acv::PhysicalReadbackProviders providers(mutex);
  unsigned confirmations=0;
  bool published=false;
  for(unsigned i=0;i<64;++i) assert(providers.Register({{i*16,4},[]{assert(false);return false;},
      [&]{++confirmations;return published;},[&]{return published;}}));
  assert(!providers.Register({{0x10000,4},[]{return true;},[]{return true;},{}}));
  published=true;
  // Capacity can retire already-published records without waiting on GPU again.
  assert(providers.Register({{0x10000,4},[]{return true;},[]{return true;},{}}));
  assert(confirmations==64 && providers.pending()==1);
  auto lock=std::unique_lock<std::recursive_mutex>(mutex);
  assert(providers.Synchronize(lock,{0x10000,4}));
  bool first=false,second=true;
  assert(providers.RegisterLocked(lock,{{0,4},[]{return true;},[]{return true;},[&]{return first;}}));
  assert(providers.RegisterLocked(lock,{{4,4},[]{return true;},[]{return true;},[&]{return second;}}));
  assert(providers.CollectPublished(lock) && providers.pending()==2);
  first=true;
  assert(providers.CollectPublished(lock) && providers.pending()==0);
  assert(providers.CancelAfterDeviceStopped());lock.unlock();
  unsigned waits=0,copies=0;
  acv::DeferredReadbackQueue queue([&](uint64_t ticket){assert(ticket==1||ticket==2);++waits;return true;},1,4);
  assert(queue.Enqueue({{0,4},1,1,[&]{++copies;return true;}}));
  assert(queue.PrepareForEnqueue(4) && waits==1 && copies==1 && queue.pending()==0);
  // New staging contents may only be recorded after this admission point.
  assert(queue.Enqueue({{4,4},1,2,[&]{++copies;return true;}}));
  assert(waits==1 && queue.BeforeSlotReuse(1) && waits==2 && copies==2);
  assert(!queue.PrepareForEnqueue(8));
  std::cout<<"PASS: completed provider capacity GC, ordered retirement, pre-record staging admission and slot retirement\n";
}
