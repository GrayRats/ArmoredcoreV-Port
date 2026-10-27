#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/system/xmemory.h>
#include "gpu_packet_snapshot.h"
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <future>
#include <thread>
using namespace rex::memory;
using acv::gpu_coherence::PacketOrigin;
using acv::gpu_coherence::SnapshotPacket;
int main() {
  rex::InitLogging();Memory memory;assert(memory.Initialize());
  constexpr uint32_t address=0x02000000,guest=0xE1FFF000;
  auto* heap=memory.LookupHeap(guest);
  assert(heap->AllocFixed(guest,0x20000,0x1000,kMemoryAllocationReserve|kMemoryAllocationCommit,
                          kMemoryProtectRead|kMemoryProtectWrite));
  auto* bytes=memory.TranslatePhysical(address);
  std::memset(bytes,0,0x20000);
  std::mutex gate_mutex;std::condition_variable cv;
  bool waiting=false,complete=false;unsigned published=0;
  assert(memory.RegisterExplicitPhysicalReadback({{address,4},[&] {
    std::unique_lock gate(gate_mutex);waiting=true;cv.notify_all();
    cv.wait(gate,[&]{return complete;});return true;
  },[&]{uint32_t value=0xABCDEF01;std::memcpy(bytes,&value,4);++published;return true;}}));
  {
    Memory::PhysicalGpuWriteScope scope(memory,address,0x1000);
    assert(scope.valid() && published==0); // Preparation waits only for host IO.
    std::thread completion([&] {
      {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return waiting;});}
      auto lock=rex::thread::global_critical_region::AcquireDirect();
      {std::lock_guard gate(gate_mutex);complete=true;}cv.notify_all();
    });
    uint32_t value=0;
    assert(memory.ReadPhysicalCpu(address,&value,4) && value==0xABCDEF01 && published==1);
    completion.join();assert(scope.ValidateLocked());
    {Memory::PhysicalGpuWriteScope nested(memory,address,4);assert(!nested.valid());}
    assert(memory.ReadPhysicalCpu(address,&value,4) && scope.ValidateLocked());
    scope.Unlock();assert(!scope.valid());
  }
  // Hardware/host writes bypass guest read-only protection, without changing it.
  assert(heap->Protect(guest,0x20000,kMemoryProtectRead));
  const uint32_t next=0x11223344;
  assert(memory.WritePhysicalCpu(address,&next,4));
  uint32_t guest_protect=0;assert(heap->QueryProtect(guest,&guest_protect));
  assert(guest_protect==kMemoryProtectRead);
  uint32_t read=0;assert(memory.ReadPhysicalCpu(address,&read,4) && read==next);
  assert(heap->Protect(guest,0x20000,kMemoryProtectRead|kMemoryProtectWrite));
  // Byte-disjoint writes in the same host page must wait for the CPU pin:
  // callback/page-fault ranges are page granular, not byte granular.
  auto page_lock=rex::thread::global_critical_region::AcquireDirect();
  const auto pin=memory.BeginPhysicalCpuAccess(page_lock,address,4);assert(pin);page_lock.unlock();
  std::promise<void> trying,registered;auto ready=registered.get_future();
  std::thread adjacent([&] {
    trying.set_value();assert(memory.RegisterExplicitPhysicalReadback({{address+128,4},[]{return true;},[]{return true;}}));
    registered.set_value();
  });
  trying.get_future().wait();
  assert(ready.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  page_lock.lock();assert(memory.EndPhysicalCpuAccess(page_lock,pin));page_lock.unlock();adjacent.join();
  assert(memory.SynchronizePhysicalCpu(address+128,4));
  // A private CPU read must still wait for an unlocked host IO owner. The
  // conflict-free fast path may never bypass a same-page reservation.
  auto io_lock=rex::thread::global_critical_region::AcquireDirect();
  auto io_pin=memory.BeginPhysicalCpuAccess(io_lock,address,4);assert(io_pin);io_lock.unlock();
  uint32_t owner_read=0;assert(!memory.ReadPhysicalCpu(address,&owner_read,4));
  std::promise<uint32_t> io_result;auto io_ready=io_result.get_future();
  std::promise<void> reader_started;
  std::thread io_reader([&]{reader_started.set_value();uint32_t value=0;assert(memory.ReadPhysicalCpu(address,&value,4));io_result.set_value(value);});
  reader_started.get_future().wait();
  assert(io_ready.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  io_lock.lock();const uint32_t io_value=0x56781234;std::memcpy(bytes,&io_value,4);
  assert(memory.EndPhysicalCpuWriteAccess(io_lock,io_pin,address,4));io_lock.unlock();
  assert(io_ready.get()==io_value);io_reader.join();
  // Fast reads retain committed-page validation instead of touching a hole.
  assert(heap->Decommit(guest+0x10000,0x1000));
  uint32_t hole=0;assert(!memory.ReadPhysicalCpu(address+0x10000,&hole,4));
  assert(heap->AllocFixed(guest+0x10000,0x1000,0x1000,kMemoryAllocationCommit,
                         kMemoryProtectRead|kMemoryProtectWrite));
  // Packet types, wrap, header stability and inline shader physical provenance.
  for(uint32_t type=0;type<4;++type) {
    const uint32_t body_words=type==2?0:type==1?2:3;
    const uint32_t packet=(type<<30)|(type==1||type==2?0:(body_words-1)<<16)|0x100;
    for(uint32_t start:{0u,60u}) {
      constexpr uint32_t capacity=64;
      uint32_t header=rex::byte_swap(packet);std::memcpy(bytes+start,&header,4);
      for(uint32_t i=0;i<body_words;++i) {
        const uint32_t value=rex::byte_swap(0x1000u+i);
        std::memcpy(bytes+(start+4+i*4)%capacity,&value,4);
      }
      RingBuffer source(bytes,capacity);source.set_read_offset(start);
      source.set_write_offset((start+4+body_words*4)%capacity);
      std::vector<uint8_t> captured;uint32_t base,source_start;
      assert(SnapshotPacket(memory,source,captured,base,source_start));
      assert(base==address && source_start==start && captured.size()==(body_words+2)*4);
      RingBuffer local(captured.data(),captured.size());local.set_write_offset(captured.size()-4);
      PacketOrigin origin(local,base,capacity,source_start);
      assert(local.ReadAndSwap<uint32_t>()==packet);
      assert(PacketOrigin::PhysicalAddress(local)==address+(start+4)%capacity);
      for(uint32_t i=0;i<body_words;++i) assert(local.ReadAndSwap<uint32_t>()==0x1000u+i);
      {
        RingBuffer nested(captured.data(),captured.size());
        PacketOrigin inner(nested,address+0x1000,capacity,0);
        assert(PacketOrigin::PhysicalAddress(nested)==address+0x1000);
        assert(PacketOrigin::PhysicalAddress(local)==address+(start+4+body_words*4)%capacity);
      }
      assert(source.read_offset()==(start+4+body_words*4)%capacity);
    }
  }
  // A body produced by a pending GPU copy must be seen on a wrapping packet.
  const uint32_t header=rex::byte_swap(0xC0011000u);std::memcpy(bytes+60,&header,4);
  assert(memory.RegisterExplicitPhysicalReadback({{address,8},[]{return true;},[&] {
    uint32_t values[]={rex::byte_swap(7u),rex::byte_swap(9u)};std::memcpy(bytes,values,8);return true;
  }}));
  RingBuffer wrap(bytes,64);wrap.set_read_offset(60);wrap.set_write_offset(8);
  std::vector<uint8_t> captured;uint32_t base,start;
  assert(SnapshotPacket(memory,wrap,captured,base,start));
  uint32_t body[2];std::memcpy(body,captured.data()+4,8);
  assert(rex::byte_swap(body[0])==7 && rex::byte_swap(body[1])==9);
  // Malformed packet must leave the source reader untouched.
  const uint32_t oversized=rex::byte_swap(0xC0101000u);std::memcpy(bytes,&oversized,4);
  RingBuffer bad(bytes,64);bad.set_write_offset(8);
  assert(!SnapshotPacket(memory,bad,captured,base,start) && bad.read_offset()==0);
  assert(heap->Release(guest));
  assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  std::cout<<"PASS: borrowed GPU scope unlock during wait, nested scope safety, readonly hardware write, coherent packet types/wrap/inline addresses and pending GPU body\n";
}
