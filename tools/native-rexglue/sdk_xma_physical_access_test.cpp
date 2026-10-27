// Actual XMA Work/RAII source with codec operations stubbed; compiled SDK Memory.
#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/audio/xma/context.h>
#include <rex/memory/ring_buffer.h>
#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
namespace rex::audio {
struct MockXmaContext {
  static constexpr uint32_t kOutputBytesPerBlock=XmaContext::kOutputBytesPerBlock;
  static constexpr uint32_t kBytesPerPacket=XmaContext::kBytesPerPacket;
  memory::Memory* memory_;
  uint32_t guest_;
  bool enabled_=true;
  std::mutex lock_;
  int32_t remaining_subframe_blocks_in_output_buffer_=0;
  uint32_t current_frame_remaining_subframes_=0;
  std::function<void(XMA_CONTEXT_DATA*)> decode;
  memory::Memory* memory() {return memory_;}
  uint32_t guest_ptr() const {return guest_;}
  bool is_allocated() const {return true;}
  bool is_enabled() const {return enabled_;}
  void set_is_enabled(bool v) {enabled_=v;}
  memory::RingBuffer PrepareOutputRingBuffer(XMA_CONTEXT_DATA* data) {
    remaining_subframe_blocks_in_output_buffer_=data->output_buffer_block_count;
    return memory::RingBuffer(memory_->TranslatePhysical(data->output_buffer_ptr),
                              data->output_buffer_block_count*kOutputBytesPerBlock);
  }
  void Decode(XMA_CONTEXT_DATA* data) {decode(data);current_frame_remaining_subframes_=1;}
  void Consume(memory::RingBuffer* ring,XMA_CONTEXT_DATA* data) {
    uint8_t samples[kOutputBytesPerBlock];std::memset(samples,0x55,sizeof(samples));
    ring->Write(samples,sizeof(samples));--remaining_subframe_blocks_in_output_buffer_;
    current_frame_remaining_subframes_=0;data->input_buffer_0_valid=0;data->input_buffer_1_valid=0;
  }
  void StoreContextMerged(const XMA_CONTEXT_DATA& data,const XMA_CONTEXT_DATA&,void* ptr) {
    auto copy=data;copy.Store(ptr);
  }
  bool Work();
};
#include "acv_xma_work.inc"
} // namespace rex::audio
int main() {
  using namespace rex::memory;using namespace rex::audio;
  rex::InitLogging();Memory memory;assert(memory.Initialize());
  constexpr uint32_t physical=0x02000000,guest=0xE1FFF000;
  auto* heap=memory.LookupHeap(guest);
  assert(heap->AllocFixed(guest,0x4000,0x1000,kMemoryAllocationReserve|kMemoryAllocationCommit,
                          kMemoryProtectRead|kMemoryProtectWrite));
  auto* output=memory.TranslatePhysical<uint8_t*>(physical);
  std::memset(output,0,0x4000);
  uint8_t blank[64]={};XMA_CONTEXT_DATA data(blank);
  data.output_buffer_ptr=physical;data.output_buffer_block_count=2;data.output_buffer_valid=1;
  data.input_buffer_0_ptr=physical+0x1000;data.input_buffer_0_packet_count=1;data.input_buffer_0_valid=1;
  data.input_buffer_1_ptr=physical+0x2000;data.input_buffer_1_packet_count=1;data.input_buffer_1_valid=1;
  data.subframe_decode_count=1;
  constexpr uint32_t context_guest=guest+0x3000;
  data.Store(memory.TranslateVirtual(context_guest));
  std::mutex gate_mutex;std::condition_variable cv;
  bool awaiting=false,complete=false,decoding=false,finish_decode=false;
  unsigned publications=0;
  assert(memory.RegisterExplicitPhysicalReadback({{physical+0x1000,0x800},[&] {
    std::unique_lock gate(gate_mutex);awaiting=true;cv.notify_all();
    cv.wait(gate,[&]{return complete;});return true;
  },[&]{std::memset(output+0x1000,0x34,0x800);++publications;return true;}}));
  MockXmaContext context{&memory,context_guest};
  context.decode=[&](XMA_CONTEXT_DATA* current) {
    assert(current->GetCurrentInputBufferAddress()==physical+0x1000 && output[0x1000]==0x34);
    assert(publications==3 && output[0]==0x11);
    std::unique_lock gate(gate_mutex);decoding=true;cv.notify_all();
    cv.wait(gate,[&]{return finish_decode;});
  };
  std::thread decoder([&]{assert(context.Work());});
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return awaiting;});}
  // Registration for an earlier output range must remain possible while the
  // later input range's GPU completion is awaited: no partial reservation.
  assert(memory.RegisterExplicitPhysicalReadback({{physical,0x1000},[]{return true;},[&]{
    std::memset(output,0x11,0x1000);++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(physical,0x1000,true,true);
  // Context was loaded before the wait. This watch faults during Store under
  // the decoder's own reservation and must not wait for that same reservation.
  assert(memory.RegisterExplicitPhysicalReadback({{physical+0x3000,64},[]{return true;},[&]{
    ++publications;return true;
  }}));
  memory.EnablePhysicalMemoryAccessCallbacks(physical+0x3000,64,false,true);
  {std::lock_guard gate(gate_mutex);complete=true;}cv.notify_all();
  {std::unique_lock gate(gate_mutex);cv.wait(gate,[&]{return decoding;});}
  std::promise<void> trying,registered;auto registration=registered.get_future();
  std::thread gpu([&] {
    auto lock=rex::thread::global_critical_region::AcquireDirect();trying.set_value();
    assert(memory.RegisterExplicitPhysicalReadbackLocked(lock,{{physical,256},[]{return true;},[&]{
      assert(output[0]==0x55);std::memset(output,0x77,256);return true;
    }}));registered.set_value();
  });
  trying.get_future().wait();
  assert(registration.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
  {auto lock=rex::thread::global_critical_region::AcquireDirect();
   std::lock_guard gate(gate_mutex);finish_decode=true;}
  cv.notify_all();decoder.join();gpu.join();
  XMA_CONTEXT_DATA finished(memory.TranslateVirtual(context_guest));
  assert(!finished.input_buffer_0_valid && !finished.input_buffer_1_valid);
  assert(output[0]==0x55 && output[255]==0x55 && output[256]==0x11);
  {auto lock=rex::thread::global_critical_region::AcquireDirect();
   assert(memory.SynchronizeExplicitPhysicalReadback(lock,physical,256));}
  assert(output[0]==0x77);
  assert(heap->Release(guest));
  assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  std::cout<<"PASS: actual XMA Work/RAII, atomic input/output/context pins, no partial pin GPU cycle, unlocked decoder, owner context fault, output publication order\n";
}
