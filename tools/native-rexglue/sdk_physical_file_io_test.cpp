// The driver includes the actual SDK XFile::ReadInternal body with only its
// class name and completion sink replaced. Memory is the compiled SDK instance.
#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/system/xmemory.h>
#include <rex/system/xtypes.h>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <span>
#include <stdexcept>
#include <thread>
namespace io_test {
using namespace rex;
namespace memory=rex::memory;
struct MockFile {
  std::function<X_STATUS(std::span<uint8_t>,size_t,size_t*)> read;
  X_STATUS ReadSync(std::span<uint8_t> buffer,size_t offset,size_t* actual) {
    return read(buffer,offset,actual);
  }
};
struct MockCompletion {struct IONotification {uint32_t apc_context,num_bytes;X_STATUS status;};};
struct MockEvent { void Set() {assert(false);} };
struct TestFile {
  memory::Memory* memory_;
  MockFile* file_;
  uint64_t position_=0;
  MockEvent event;
  MockEvent* async_event_=&event;
  memory::Memory* memory() {return memory_;}
  void NotifyIOCompletionPorts(MockCompletion::IONotification) {assert(false);}
  X_STATUS ReadInternal(uint32_t,uint32_t,uint64_t,uint32_t*,uint32_t,bool);
};
#include "acv_xfile_read_internal.inc"
} // namespace io_test
int main() {
  using namespace rex::memory;
  using namespace io_test;
  rex::InitLogging();Memory memory;assert(memory.Initialize());
  constexpr uint32_t guest=0xE1FFF000,physical=0x02000000;
  auto* heap=memory.LookupHeap(guest);
  assert(heap->AllocFixed(guest,0x1000,0x1000,
      kMemoryAllocationReserve|kMemoryAllocationCommit,kMemoryProtectRead|kMemoryProtectWrite));
  auto* bytes=memory.TranslatePhysical<uint8_t*>(physical);
  std::memset(bytes,0,0x1000);
  unsigned publications=0,invalidations=0;
  auto handle=memory.RegisterPhysicalMemoryInvalidationCallback(
      [](void* context,uint32_t,uint32_t,bool) {
        ++*static_cast<unsigned*>(context);return std::pair<uint32_t,uint32_t>{0,UINT32_MAX};
      },&invalidations);
  assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&] {
    std::memset(bytes,0x34,0x1000);++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,true,true);
  std::mutex gate_mutex;std::condition_variable cv;
  bool io_entered=false,io_continue=false;
  MockFile backend;
  backend.read=[&](std::span<uint8_t> buffer,size_t offset,size_t* actual) {
    assert(offset==9 && publications==1 && bytes[0]==0x34);
    std::unique_lock gate(gate_mutex);io_entered=true;cv.notify_all();
    cv.wait(gate,[&]{return io_continue;});
    std::memset(buffer.data(),0x55,4);*actual=4;return X_STATUS_SUCCESS;
  };
  TestFile file{&memory,&backend};uint32_t actual=0;
  std::thread reader([&]{assert(file.ReadInternal(guest,4,9,&actual,0,false)==X_STATUS_SUCCESS);});
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return io_entered;});}
  std::promise<void> registering,registered;
  auto registration=registered.get_future();
  std::thread gpu([&] {
    registering.set_value();
    assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&] {
      assert(bytes[0]==0x55 && bytes[4]==0x34 && invalidations>0);
      std::memset(bytes,0x77,0x1000);++publications;return true;
    }}));
    registered.set_value();
  });
  registering.get_future().wait();
  assert(registration.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  // The file IO reservation must not retain the SDK global mutex.
  {auto lock=rex::thread::global_critical_region::AcquireDirect();
   std::lock_guard gate(gate_mutex);io_continue=true;}
  cv.notify_all();reader.join();gpu.join();
  assert(actual==4 && file.position_==13 && bytes[0]==0x55 && bytes[4]==0x34);
  {auto lock=rex::thread::global_critical_region::AcquireDirect();
   assert(memory.SynchronizeExplicitPhysicalReadback(lock,physical,4));}
  assert(bytes[0]==0x77 && publications==2);
  // A failing backend may still modify bytes. Invalidate these writes too.
  const auto previous_invalidations=invalidations;
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,true,false);
  backend.read=[](std::span<uint8_t> buffer,size_t,size_t* actual) {
    buffer[0]=0x66;*actual=1;return X_STATUS_UNSUCCESSFUL;
  };
  assert(file.ReadInternal(guest,4,0,&actual,0,false)==X_STATUS_UNSUCCESSFUL);
  assert(actual==1 && bytes[0]==0x66 && bytes[1]==0x77 && invalidations>previous_invalidations);
  assert(file.position_==13);
  const auto before_exception=invalidations;
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,true,false);
  backend.read=[](std::span<uint8_t> buffer,size_t,size_t*) -> X_STATUS {
    buffer[0]=0x88;throw std::runtime_error("synthetic disk exception");
  };
  bool threw=false;
  try {file.ReadInternal(guest,4,0,&actual,0,false);} catch(const std::runtime_error&) {threw=true;}
  assert(threw && bytes[0]==0x88 && invalidations>before_exception);
  assert(memory.RegisterExplicitPhysicalReadback({{physical,4},[]{return true;},[]{return true;}}));
  {auto lock=rex::thread::global_critical_region::AcquireDirect();
   assert(memory.SynchronizeExplicitPhysicalReadback(lock,physical,4));}
  memory.UnregisterPhysicalMemoryInvalidationCallback(handle);
  assert(heap->Release(guest));
  assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  std::cout<<"PASS: actual XFile read body, pre-write GPU drain, unlocked disk IO, overlapping registration blocked, partial bytes preserved, post-write invalidation including failed IO\n";
}
