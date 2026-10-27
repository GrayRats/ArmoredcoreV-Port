#include "deferred_readback_queue.h"
#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>
using Q = acv::DeferredReadbackQueue;
int main() {
  std::array<unsigned char, 64> memory{};
  std::vector<uint64_t> waits;
  Q q([&](uint64_t t) { waits.push_back(t); return true; });
  auto job = [&](uint32_t address, uint32_t bytes, uint64_t slot, uint64_t ticket,
                 unsigned char value) {
    return Q::Job{{address, bytes}, slot, ticket, [&,address,bytes,value]() {
      std::fill(memory.begin()+address, memory.begin()+address+bytes, value); return true;
    }};
  };
  assert(q.Enqueue(job(0,16,1,1,0x11)));
  assert(q.Enqueue(job(8,16,2,2,0x22)));
  assert(q.Enqueue(job(40,8,3,3,0x33)));
  assert(memory[0] == 0 && waits.empty()); // no wait/copy at enqueue
  assert(q.BeforeCpuAccess({30,1}) && waits.empty()); // unrelated CPU data
  assert(q.BeforeCpuAccess({8,1}));
  assert(waits == std::vector<uint64_t>{2}); // batch only newest selected ticket
  assert(memory[0] == 0x11 && memory[8] == 0x22 && memory[23] == 0x22);
  assert(memory[40] == 0 && q.pending() == 1 && q.pending_bytes() == 8);
  // A CPU write is preceded by synchronization, preventing old data overwrites.
  assert(q.BeforeCpuAccess({40,1})); memory[40] = 0xAA;
  assert(q.Drain() && memory[40] == 0xAA);
  assert(q.Enqueue(job(0,8,1,4,0x44)));
  assert(!q.Enqueue(job(0,8,1,5,0x55))); // misuse: slot not retired before recording
  assert(q.BeforeSlotReuse(1) && memory[0] == 0x44);
  assert(q.Enqueue(job(0,8,1,5,0x55)) && q.Drain());
  assert(memory[0] == 0x55);
  assert(!q.Enqueue(job(0,8,2,4,0))); // ticket regression
  assert(!q.BeforeCpuAccess({0x1FFFFFFF,2}));
  assert(!q.BeforeCpuAccess({0,0}));
  bool foreign = true;
  std::thread worker([&] { foreign = q.Drain(); }); worker.join();
  assert(!foreign && !q.failed());
  // Bounded queue flushes instead of dropping jobs.
  Q bounded([&](uint64_t t) { waits.push_back(t); return true; },1,16);
  assert(bounded.Enqueue(job(0,8,1,1,1)));
  assert(bounded.Enqueue(job(8,8,2,2,2)));
  assert(bounded.pending() == 1 && memory[0] == 1);
  assert(bounded.Drain() && memory[8] == 2);
  // Capacity waits only for the oldest prefix needed, leaving newer copies
  // in flight. Byte and job-count limits are enforced independently.
  {
    std::vector<unsigned> published;std::vector<uint64_t> tickets;
    Q byte_limited([&](uint64_t t){tickets.push_back(t);return true;},8,16);
    auto add=[&](uint32_t bytes,uint64_t slot,uint64_t ticket){
      return byte_limited.Enqueue({{0,bytes},slot,ticket,[&,slot]{published.push_back(unsigned(slot));return true;}});
    };
    assert(add(4,1,1) && add(8,2,2) && add(4,3,3));
    assert(byte_limited.PrepareForEnqueue(8));
    assert(tickets==std::vector<uint64_t>{2} && published==std::vector<unsigned>({1,2}));
    assert(byte_limited.pending()==1 && byte_limited.pending_bytes()==4);
    assert(add(8,4,4) && byte_limited.pending()==2 && byte_limited.pending_bytes()==12);
    assert(byte_limited.Drain());
    assert(published==std::vector<unsigned>({1,2,3,4}));
  }
  {
    std::vector<uint64_t> tickets;std::vector<unsigned> published;
    Q count_limited([&](uint64_t t){tickets.push_back(t);return true;},3,64);
    for(unsigned i=1;i<=3;++i)assert(count_limited.Enqueue({{0,4},i,i,[&,i]{published.push_back(i);return true;}}));
    assert(count_limited.PrepareForEnqueue(4));
    assert(tickets==std::vector<uint64_t>{1} && published==std::vector<unsigned>{1});
    assert(count_limited.pending()==2 && count_limited.pending_bytes()==8);
    assert(count_limited.Drain());
  }
  {
    // Equal tickets may belong to separate copies in one submission. Retire a
    // slot prefix, not every copy sharing the completed fence.
    std::vector<unsigned> published;std::vector<uint64_t> tickets;
    Q same_ticket([&](uint64_t t){tickets.push_back(t);return true;},2,8);
    assert(same_ticket.Enqueue({{0,4},1,1,[&]{published.push_back(1);return true;}}));
    assert(same_ticket.Enqueue({{4,4},2,1,[&]{published.push_back(2);return true;}}));
    assert(same_ticket.Enqueue({{8,4},3,2,[&]{published.push_back(3);return true;}}));
    assert(tickets==std::vector<uint64_t>{1} && published==std::vector<unsigned>{1});
    assert(same_ticket.pending()==2 && same_ticket.pending_bytes()==8);
    assert(same_ticket.BeforeCpuAccess({4,4}) && published==std::vector<unsigned>({1,2}));
    assert(same_ticket.Drain() && published==std::vector<unsigned>({1,2,3}));
  }
  {
    std::vector<uint64_t> tickets;unsigned copies=0;
    Q capacity_failure([&](uint64_t t){tickets.push_back(t);return false;},2,8);
    assert(capacity_failure.Enqueue({{0,4},1,1,[&]{++copies;return true;}}));
    assert(capacity_failure.Enqueue({{4,4},2,2,[&]{++copies;return true;}}));
    assert(!capacity_failure.PrepareForEnqueue(4));
    assert(tickets==std::vector<uint64_t>{1} && copies==0 && capacity_failure.failed());
    assert(capacity_failure.pending()==2 && capacity_failure.pending_bytes()==8);
    assert(capacity_failure.CancelAfterDeviceStopped());
  }
  // Failed completion must never publish stale bytes.
  int published = 0;
  Q failed([](uint64_t) { return false; });
  assert(failed.Enqueue({{0,4},1,1,[&] { ++published; return true; }}));
  assert(!failed.Drain() && failed.failed() && published == 0);
  assert(!failed.BeforeCpuAccess({0,4}) && failed.pending() == 1);
  assert(failed.CancelAfterDeviceStopped() && failed.pending() == 0);
  // Invalidate/publication failure prevents later copies too.
  Q invalid([](uint64_t) { return true; });
  assert(invalid.Enqueue({{0,4},1,1,[] { return false; }}));
  assert(invalid.Enqueue({{4,4},2,2,[&] { ++published; return true; }}));
  assert(!invalid.Drain() && invalid.failed() && published == 0);
  assert(invalid.CancelAfterDeviceStopped());
  // Reentrant worker cleanup cannot mutate the in-progress list.
  Q* active = nullptr;
  Q recursive([&](uint64_t) { assert(!active->Drain()); return true; }); active=&recursive;
  assert(recursive.Enqueue(job(0,4,1,1,9)) && recursive.Drain());
  Q throwing([](uint64_t)->bool { throw std::runtime_error("device stopped"); });
  assert(throwing.Enqueue(job(0,4,1,1,9)) && !throwing.Drain() && throwing.failed());
  assert(throwing.CancelAfterDeviceStopped());
  std::cout << "PASS: deferred copies, overlap ordering, selective wait, slot lifetime, CPU writes, bounds, failure and reentrancy\n";
}
