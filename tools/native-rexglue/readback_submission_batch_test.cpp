#include "readback_batch_test_support.h"
#include "readback_submission_scope.h"
struct Fixture {
  HostMemory memory;Dispatcher dispatch;
  uint64_t ticket=1,submits=0,waits=0;bool fail=false;
  Bridge bridge;
  Fixture(size_t jobs=64,uint64_t bytes=64ULL<<20):
    bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},
      [&](uint64_t value){assert(value<ticket);++waits;return true;},jobs,bytes) {
    bridge.EnableBatching([&](uint64_t value){assert(value==ticket);return Submit();});
    bridge.EnableStatistics();
  }
  bool Submit() {
    bool unlocked=false;
    std::thread check([&]{unlocked=memory.mutex.try_lock();if(unlocked) memory.mutex.unlock();});
    check.join();assert(unlocked); // Submit never owns the memory-system mutex.
    acv::ReadbackSubmissionScope scope(&bridge,ticket);
    if(fail) return false;
    ++ticket;++submits;
    return scope.Submitted();
  }
  Bridge::StatePtr Prepare(uint32_t address,uint32_t bytes,uint64_t slot) {
    assert(bridge.Prepare(bytes));assert(bridge.RetireSlot(slot));
    auto state=bridge.MakeState({address,bytes},slot);assert(state);
    auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);
    assert(bridge.RegisterLocked(lock,state));return state;
  }
  Bridge::StatePtr Add(uint32_t address,uint32_t bytes,uint64_t slot,uint8_t value) {
    auto state=Prepare(address,bytes,slot);
    assert(bridge.Record(state,ticket,[&,address,bytes,value]{
      assert(address+bytes<=sizeof(memory.data));
      std::memset(reinterpret_cast<uint8_t*>(memory.data)+address,value,bytes);return true;}));
    return state;
  }
  ~Fixture() {bridge.Close();assert(bridge.CancelAfterDeviceStopped());}
};
int main() {
  for(bool other_thread:{false,true}) {
    Fixture f;auto a=f.Add(0,8,1,0x11);auto b=f.Add(4,8,2,0x22);
    assert(f.submits==0 && f.bridge.recorded()==2);
    uint8_t got[12]={};
    if(other_thread) {
      std::thread cpu([&]{assert(f.memory.Read(0,got,12));});
      f.dispatch.Wait();f.dispatch.Pump();cpu.join();
    } else assert(f.memory.Read(0,got,12));
    assert(f.submits==1 && f.waits==1 && f.bridge.pending()==0);
    for(int i=0;i<12;++i) assert(got[i]==(i<4?0x11:0x22));
    assert(a->phase.load()==Bridge::Phase::Published && b->ready());
    // CPU partial writes after synchronization are not overwritten by old jobs.
    {auto lock=std::unique_lock<std::recursive_mutex>(f.memory.mutex);
      assert(f.memory.providers.Synchronize(lock,{2,6}));
      std::memset(reinterpret_cast<uint8_t*>(f.memory.data)+2,0x44,6);}
    assert(f.bridge.Drain());
    auto* data=reinterpret_cast<uint8_t*>(f.memory.data);
    for(int i=2;i<8;++i) assert(data[i]==0x44);
  }
  {
    Fixture f;auto first=f.Add(0,4,1,1);auto preparing=f.Prepare(4,4,2);
    assert(f.Submit()); // Draw/primary-end/Swap close while next provider prepares.
    assert(first->phase.load()==Bridge::Phase::Submitted);
    assert(preparing->phase.load()==Bridge::Phase::Preparing && !preparing->failed);
    assert(f.bridge.Record(preparing,f.ticket,[]{return true;}));
    assert(f.bridge.Drain() && f.submits==2);
  }
  {
    Fixture f;for(int i=0;i<4;++i) f.Add(i*4,4,i+1,uint8_t(i));
    assert(f.submits==1 && f.bridge.recorded()==0 && f.bridge.pending()==4);
    assert(f.bridge.statistics().max_jobs==4);
    assert(f.bridge.Drain());
  }
  {
    // Combined count/bytes reservation: the fourth cannot overbook a 3/12 queue.
    Fixture f(3,12);for(int i=0;i<3;++i) f.Add(i*4,4,i+1,uint8_t(i));
    assert(f.submits==0);f.Add(12,4,4,4);
    assert(f.submits==1 && f.waits==1 && f.bridge.pending()==3 && f.bridge.pending_bytes()==12);
    assert(f.bridge.Drain() && f.submits==2);
  }
  {
    Fixture f;auto a=f.Prepare(0,9<<20,1);
    assert(f.bridge.Record(a,f.ticket,[]{return true;}));
    auto b=f.Prepare(9<<20,9<<20,2); // byte cap flush, even below job cap.
    assert(f.submits==1);
    assert(f.bridge.Record(b,f.ticket,[]{return true;}));
    assert(f.bridge.Drain() && f.submits==2);
  }
  {
    Fixture f;f.Add(0,4,1,1);assert(f.bridge.RetireSlot(1));
    assert(f.submits==1 && f.waits==1 && f.bridge.pending()==0);
  }
  for(bool close_only:{false,true}) {
    Fixture f;auto state=f.Prepare(0,4,1);
    auto lease=std::make_shared<int>(7);std::weak_ptr<int> held=lease;
    assert(f.bridge.Record(state,f.ticket,[lease]{return true;}));lease.reset();
    if(close_only) f.bridge.Close();
    else {f.fail=true;assert(!f.bridge.Drain());}
    assert(state->failed && !held.expired() && f.bridge.recorded()==1);
    assert(f.bridge.CancelAfterDeviceStopped() && held.expired());
  }
  {
    // Real queue busy boundary injects a successful GPU submit whose commit
    // must fail explicitly. Neither callbacks nor retained staging may vanish.
    HostMemory memory;Dispatcher dispatch;Bridge* owner=nullptr;uint64_t ticket=1;
    bool injected=false;
    Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},
      [&](uint64_t){acv::ReadbackSubmissionScope scope(owner,ticket);
        injected=true;return scope.Submitted();});owner=&bridge;
    bridge.EnableBatching([&](uint64_t t){acv::ReadbackSubmissionScope scope(&bridge,t);++ticket;return scope.Submitted();});
    auto add=[&](uint64_t slot) {assert(bridge.Prepare(4));auto state=bridge.MakeState({uint32_t(slot*4),4},slot);
      {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
      assert(bridge.Record(state,ticket,[]{return true;}));return state;};
    add(1);assert(bridge.FlushRecorded(Bridge::FlushReason::Drain));auto second=add(2);
    assert(!bridge.RetireSlot(1) && injected && second->failed && bridge.recorded()==1);
    assert(bridge.pending()==2);bridge.Close();assert(bridge.CancelAfterDeviceStopped());
  }
  {
    Fixture f;f.Add(0,4,1,1);
    // Recursive flush is rejected rather than submitting twice/deadlocking.
    // Direct shared scope failure also models every pre-submit VkResult failure.
    {acv::ReadbackSubmissionScope scope(&f.bridge,f.ticket);}
    assert(!f.bridge.Drain() && f.bridge.recorded()==1 && f.submits==0);
  }
  {
    Fixture f;auto state=f.Add(0,4,1,1);
    assert(!f.bridge.SubmissionSucceeded(f.ticket+1));
    assert(state->failed && f.bridge.recorded()==1 && !f.bridge.Prepare(4));
  }
  {
    Fixture f;auto state=f.Prepare(0,20<<20,1);
    assert(f.bridge.Record(state,f.ticket,[]{return true;}));
    assert(f.submits==1 && f.bridge.recorded()==0); // Oversized single is immediate.
    assert(f.bridge.Drain());
  }
  {
    acv::DeferredReadbackQueue queue([](uint64_t){return true;});
    auto lease=std::make_shared<int>(3);std::weak_ptr<int> held=lease;
    std::list<acv::DeferredReadbackQueue::Job> jobs;
    jobs.push_back({{0,4},1,1,[lease]{return true;}});
    jobs.push_back({{4,4},1,1,[]{return true;}});lease.reset();
    assert(!queue.AppendSubmittedNoWait(jobs) && jobs.size()==2 && !held.expired());
    assert(queue.pending()==0);jobs.clear();assert(held.expired());
  }
  {
    HostMemory memory;Dispatcher dispatch;Bridge* owner=nullptr;
    Bridge bridge(memory,memory.mutex,[&](auto fn){dispatch.Post(std::move(fn));},[](uint64_t){return true;});
    owner=&bridge;
    bridge.EnableBatching([&](uint64_t){return owner->FlushRecorded(Bridge::FlushReason::Cpu);});
    auto state=bridge.MakeState({0,4},1);
    {auto lock=std::unique_lock<std::recursive_mutex>(memory.mutex);assert(bridge.RegisterLocked(lock,state));}
    assert(bridge.Record(state,1,[]{return true;}));
    assert(!bridge.Drain() && state->failed && bridge.recorded()==1);
    bridge.Close();assert(bridge.CancelAfterDeviceStopped());
  }
  {
    Fixture f;auto state=f.Add(0,4,1,1);bool result=true;
    std::thread cpu([&]{uint32_t value;result=f.memory.Read(0,&value,4);});
    f.dispatch.Wait();f.bridge.Close();cpu.join();
    assert(!result && state->failed && f.submits==0 && f.bridge.recorded()==1);
    f.dispatch.Pump();assert(f.bridge.CancelAfterDeviceStopped());
  }
  std::cout<<"PASS batch lifecycle: CPU inline/thread, partial generations/write, preparing exclusion, caps, slot/drain/external close, submit/commit failure and retained leases\n";
}
