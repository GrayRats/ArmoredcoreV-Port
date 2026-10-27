#include "general_readback_bridge.h"
#include "physical_readback_providers.h"
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <thread>
struct HostMemory {
  using PhysicalReadbackProvider=acv::PhysicalReadbackProviders::Provider;
  std::recursive_mutex mutex;
  acv::PhysicalReadbackProviders providers{mutex};
  uint32_t data[16]={};
  bool RegisterExplicitPhysicalReadbackLocked(std::unique_lock<std::recursive_mutex>& lock,
                                             PhysicalReadbackProvider provider) {
    return providers.RegisterLocked(lock,std::move(provider));
  }
  bool CollectPublishedPhysicalReadbacks() {
    auto lock=std::unique_lock<std::recursive_mutex>(mutex);return providers.CollectPublished(lock);
  }
  bool Read(uint32_t offset,void* destination,uint32_t bytes) {
    assert(offset+bytes<=sizeof(data));auto lock=std::unique_lock<std::recursive_mutex>(mutex);
    if(!providers.Synchronize(lock,{offset,bytes})) return false;
    std::memcpy(destination,reinterpret_cast<uint8_t*>(data)+offset,bytes);return true;
  }
};
struct Dispatcher {
  std::thread::id owner=std::this_thread::get_id();
  std::mutex mutex;std::condition_variable cv;
  std::deque<std::function<void()>> work;
  void Post(std::function<void()> fn) {
    if(std::this_thread::get_id()==owner) {fn();return;}
    {std::lock_guard lock(mutex);work.push_back(std::move(fn));}cv.notify_all();
  }
  void Wait() {std::unique_lock lock(mutex);cv.wait(lock,[&]{return !work.empty();});}
  void Pump() {
    for(;;) {std::function<void()> fn;
      {std::lock_guard lock(mutex);if(work.empty()) return;fn=std::move(work.front());work.pop_front();}
      fn();
    }
  }
};
using Bridge=acv::GeneralReadbackBridge<HostMemory>;
int main() {
  {
    HostMemory memory;Dispatcher dispatch;unsigned waits=0,copies=0;
    Bridge* bridge_ptr=nullptr;
    Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},
      [&](uint64_t ticket){assert(ticket==2 && bridge_ptr && bridge_ptr->servicing_cpu_access());++waits;return true;});
    bridge_ptr=&bridge;assert(!bridge.servicing_cpu_access());
    auto first=bridge.MakeState({0,8},1),second=bridge.MakeState({4,4},2);
    {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);
     assert(bridge.RegisterLocked(lock,first));assert(bridge.RegisterLocked(lock,second));
     assert(!bridge.RegisterLocked(lock,first));}
    assert(bridge.Commit(first,1,[&]{memory.data[0]=11;memory.data[1]=12;++copies;return true;}));
    assert(bridge.Commit(second,2,[&]{memory.data[1]=22;++copies;return true;}));
    uint32_t actual[2]={};
    std::thread cpu([&]{assert(memory.Read(0,actual,8));});
    dispatch.Wait();dispatch.Pump();cpu.join();
    assert(actual[0]==11 && actual[1]==22 && waits==1 && copies==2 && bridge.pending()==0);
    assert(memory.providers.pending()==0 && !bridge.servicing_cpu_access());
    bridge.Close();assert(bridge.CancelAfterDeviceStopped());
  }
  {
    // GPU-owner CPU reads dispatch inline through the recursive control mutex.
    HostMemory memory;Dispatcher dispatch;
    Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},[](uint64_t){return true;},1,4);
    auto state=bridge.MakeState({0,4},1);
    {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
    assert(bridge.Commit(state,1,[&]{memory.data[0]=33;return true;}));
    uint32_t actual=0;assert(memory.Read(0,&actual,4) && actual==33);
    assert(bridge.Prepare(4) && bridge.RetireSlot(1));
    bridge.Close();assert(bridge.CancelAfterDeviceStopped());
  }
  {
    // Closing after normal drain must complete CPU awaits successfully even if
    // their queued callbacks will run only after bridge destruction.
    HostMemory memory;Dispatcher dispatch;uint32_t actual=0;
    {
      Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},[](uint64_t){return true;});
      auto state=bridge.MakeState({0,4},1);
      {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
      assert(bridge.Commit(state,1,[&]{memory.data[0]=44;return true;}));
      std::thread cpu([&]{assert(memory.Read(0,&actual,4));});
      dispatch.Wait();assert(bridge.Drain());bridge.Close();cpu.join();
      assert(actual==44 && bridge.CancelAfterDeviceStopped());
    }
    dispatch.Pump(); // ASan verifies there is no raw deleted-owner call.
  }
  {
    HostMemory memory;Dispatcher dispatch;bool result=true;
    {
      Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},[](uint64_t){return true;});
      auto state=bridge.MakeState({0,4},1);
      {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
      assert(bridge.Commit(state,1,[&]{memory.data[0]=55;return true;}));
      std::thread cpu([&]{uint32_t ignored;result=memory.Read(0,&ignored,4);});
      dispatch.Wait();bridge.Close();cpu.join();
      assert(!result && memory.data[0]==0 && bridge.CancelAfterDeviceStopped());
    }
    dispatch.Pump();
  }
  for(bool wait_failure:{false,true}) {
    HostMemory memory;Dispatcher dispatch;
    Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},
                   [=](uint64_t){return !wait_failure;});
    auto lease=std::make_shared<int>(1);std::weak_ptr<int> retained=lease;
    auto state=bridge.MakeState({0,4},1);
    {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
    assert(bridge.Commit(state,1,[lease]{return false;}));lease.reset();
    uint32_t actual=0;assert(!memory.Read(0,&actual,4));
    assert(actual==0 && memory.data[0]==0 && !retained.expired());
    bridge.Close();assert(bridge.CancelAfterDeviceStopped() && retained.expired());
  }
  std::cout<<"PASS: current-generation/partial bytes, one prefix wait, inline owner, normal/failed close, late callbacks and staging retained until stop\n";
}
