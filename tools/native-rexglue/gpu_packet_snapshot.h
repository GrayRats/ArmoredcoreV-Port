#pragma once
// Snapshot one physical GPU packet before executing it. Never pin a whole ring
// while executing WAIT/indirect packets, which may depend on other CPU accesses.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <rex/memory/ring_buffer.h>
#include <rex/system/xmemory.h>
namespace acv::gpu_coherence {
class PacketOrigin {
 public:
  PacketOrigin(const rex::memory::RingBuffer& reader,uint32_t physical_base,
               uint32_t capacity,uint32_t start)
      : reader_(&reader),base_(physical_base),capacity_(capacity),start_(start),previous_(active_) {
    active_=this;
  }
  ~PacketOrigin() {active_=previous_;}
  PacketOrigin(const PacketOrigin&)=delete;
  PacketOrigin& operator=(const PacketOrigin&)=delete;
  static uint32_t PhysicalAddress(const rex::memory::RingBuffer& reader) {
    for(auto* origin=active_;origin;origin=origin->previous_)
      if(origin->reader_==&reader)
        return origin->base_+(uint64_t(origin->start_)+reader.read_offset())%origin->capacity_;
    return uint32_t(reader.read_ptr());
  }
 private:
  const rex::memory::RingBuffer* reader_;
  uint32_t base_,capacity_,start_;
  PacketOrigin* previous_;
  static inline thread_local PacketOrigin* active_=nullptr;
};
inline bool SnapshotPacket(rex::memory::Memory& memory,rex::memory::RingBuffer& source,
                           std::vector<uint8_t>& snapshot,uint32_t& physical_base,
                           uint32_t& source_start) {
  const uintptr_t host_base=reinterpret_cast<uintptr_t>(source.buffer());
  const uintptr_t memory_base=reinterpret_cast<uintptr_t>(memory.physical_membase());
  const uint32_t capacity=source.capacity();
  if(host_base<memory_base || host_base-memory_base>=0x20000000ULL ||
     capacity<4 || (capacity&3) || uint64_t(host_base-memory_base)+capacity>0x20000000ULL) return false;
  physical_base=uint32_t(host_base-memory_base);source_start=source.read_offset();
  const uint32_t available=source.read_count()?source.read_count():capacity;
  if((source_start&3) || available<4) return false;
  auto copy=[&](void* destination,uint32_t length) {
    const uint32_t first=std::min(length,capacity-source_start);
    return memory.ReadPhysicalCpu(physical_base+source_start,destination,first) &&
        (first==length || memory.ReadPhysicalCpu(physical_base,static_cast<uint8_t*>(destination)+first,
                                                length-first));
  };
  for(unsigned retry=0;retry<8;++retry) {
    uint32_t raw_header;
    if(!copy(&raw_header,4)) return false;
    const uint32_t packet=rex::byte_swap(raw_header),type=packet>>30;
    const uint32_t words=!packet || type==2?0:type==1?2:((packet>>16)&0x3FFF)+1;
    const uint32_t bytes=(words+1)*4;
    if(bytes>available || bytes>capacity) return false;
    snapshot.resize(bytes+4); // The local reader needs a spare dword to represent "full".
    if(!copy(snapshot.data(),bytes)) return false;
    uint32_t captured_header;std::memcpy(&captured_header,snapshot.data(),4);
    if(captured_header!=raw_header) continue;
    source.AdvanceRead(bytes);return true;
  }
  return false; // Concurrently rewritten command header never became stable.
}
} // namespace acv::gpu_coherence
