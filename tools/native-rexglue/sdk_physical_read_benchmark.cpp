// Host-only same-input benchmark. Does not measure gameplay or FPS.
#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/system/xmemory.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
static std::atomic<uint64_t> allocations{0};
void* operator new(std::size_t size) {
  allocations.fetch_add(1,std::memory_order_relaxed);
  if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace rex::memory;
int main(int argc,char** argv) {
  bool zero=false,pending=false;for(int i=1;i<argc;++i){zero|=std::strcmp(argv[i],"--expect-no-allocations")==0;pending|=std::strcmp(argv[i],"--pending-providers")==0;}
  rex::InitLogging();Memory memory;assert(memory.Initialize());
  constexpr uint32_t address=0x02000000,guest=0xE1FFF000,iterations=200000;
  auto* heap=memory.LookupHeap(guest);
  assert(heap->AllocFixed(guest,0x80000,0x1000,kMemoryAllocationReserve|kMemoryAllocationCommit,kMemoryProtectRead|kMemoryProtectWrite));
  auto* words=memory.TranslatePhysical<uint32_t*>(address);for(uint32_t i=0;i<1024;++i)words[i]=i;
  {Memory::PhysicalGpuWriteScope scope(memory,address,4096);assert(scope.valid());}
  if(pending)for(unsigned i=0;i<64;++i)assert(memory.RegisterExplicitPhysicalReadback(
      {{address+0x10000+i*4096,4},[]{return true;},[]{return true;}}));
  uint64_t total=0;uint32_t value=0;
  for(uint32_t i=0;i<10000;++i)assert(memory.ReadPhysicalCpu(address+(i%1024)*4,&value,4));
  for(unsigned run=0;run<3;++run) {
    const uint64_t before=allocations.load();const auto start=std::chrono::steady_clock::now();
    uint64_t sum=0;for(uint32_t i=0;i<iterations;++i){assert(memory.ReadPhysicalCpu(address+(i%1024)*4,&value,4));sum+=value;}
    const auto end=std::chrono::steady_clock::now();const uint64_t allocated=allocations.load()-before;total+=sum;
    uint64_t expected=0;for(uint32_t i=0;i<iterations;++i)expected+=i%1024;assert(sum==expected);
    if(zero)assert(allocated==0);
    std::cout<<"{\"scope\":\"host-only locked coherent word reads, no gameplay/FPS\",\"run\":"<<run
      <<",\"pending_providers\":"<<(pending?64:0)<<",\"iterations\":"<<iterations<<",\"allocations\":"<<allocated<<",\"ns_per_read\":"
      <<std::chrono::duration<double,std::nano>(end-start).count()/iterations<<",\"checksum\":"<<sum<<"}\n";
  }
  assert(total);assert(heap->Release(guest));assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
}
