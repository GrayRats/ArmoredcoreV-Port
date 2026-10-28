// Real Vulkan transfers + real SDK alias faults + production bridge.
// Numeric byte oracle only; no window, gameplay metrics or image capture.
#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/system/xmemory.h>
#include <vulkan/vulkan.h>
#include "../rexglue-sdk/src/graphics/vulkan/general_readback_bridge.h"
#include "../rexglue-sdk/src/graphics/vulkan/readback_pool_selection.h"
#include "../rexglue-sdk/src/graphics/vulkan/parallel_readback_copy.h"
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <thread>
using namespace rex::memory;
struct Dispatcher {
  std::thread::id owner=std::this_thread::get_id();
  std::mutex mutex;std::condition_variable cv;std::deque<std::function<void()>> callbacks;
  void Post(std::function<void()> callback) {
    if(owner==std::this_thread::get_id()){callback();return;}
    {std::lock_guard lock(mutex);callbacks.push_back(std::move(callback));}cv.notify_one();
  }
  void PumpOne() {
    std::function<void()> callback;
    {std::unique_lock lock(mutex);assert(cv.wait_for(lock,std::chrono::seconds(5),[&]{return !callbacks.empty();}));
     callback=std::move(callbacks.front());callbacks.pop_front();}
    callback();
  }
};
int main(int argc,char** argv) {
  const bool parallel=argc==2 && std::strcmp(argv[1],"--parallel-copy")==0;
  assert(argc==1 || parallel);
  std::unique_ptr<acv::ParallelReadbackCopy> copy_pool;
  if(parallel)copy_pool=std::make_unique<acv::ParallelReadbackCopy>(2);
  rex::InitLogging();Memory memory;assert(memory.Initialize());
  constexpr uint32_t address=0x02000000,guest=0xE1FFF000;
  const uint32_t bytes=parallel?2*1024*1024:4096;
  const uint32_t allocation=std::max(uint32_t(0x20000),3*bytes);
  auto* heap=memory.LookupHeap(guest);
  assert(heap->AllocFixed(guest,allocation,0x1000,kMemoryAllocationReserve|kMemoryAllocationCommit,
                         kMemoryProtectRead|kMemoryProtectWrite));
  std::memset(memory.TranslatePhysical(address),0,allocation);
  VkInstance instance=VK_NULL_HANDLE;VkInstanceCreateInfo ici={};ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  assert(vkCreateInstance(&ici,nullptr,&instance)==VK_SUCCESS);
  uint32_t count=0;assert(vkEnumeratePhysicalDevices(instance,&count,nullptr)==VK_SUCCESS && count);
  std::vector<VkPhysicalDevice> devices(count);assert(vkEnumeratePhysicalDevices(instance,&count,devices.data())==VK_SUCCESS);
  VkPhysicalDevice physical=VK_NULL_HANDLE;VkPhysicalDeviceProperties props{};
  for(auto candidate:devices) {
    VkPhysicalDeviceProperties candidate_props{};vkGetPhysicalDeviceProperties(candidate,&candidate_props);
    if(candidate_props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
       candidate_props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      physical=candidate;props=candidate_props;break;
    }
  }
  assert(physical!=VK_NULL_HANDLE);
  vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
  std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
  uint32_t family=0;while(family<count && !(families[family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT|VK_QUEUE_TRANSFER_BIT)))++family;
  assert(family<count);float priority=1;
  VkDeviceQueueCreateInfo qci={};qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
  VkDeviceCreateInfo dci={};dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
  VkDevice device=VK_NULL_HANDLE;assert(vkCreateDevice(physical,&dci,nullptr,&device)==VK_SUCCESS);
  VkQueue queue=VK_NULL_HANDLE;vkGetDeviceQueue(device,family,0,&queue);
  VkCommandPoolCreateInfo pci={};pci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pci.queueFamilyIndex=family;
  VkCommandPool pool=VK_NULL_HANDLE;assert(vkCreateCommandPool(device,&pci,nullptr,&pool)==VK_SUCCESS);
  struct Buffer {VkBuffer buffer;VkDeviceMemory memory;void* mapping=nullptr;};
  VkPhysicalDeviceMemoryProperties memory_props;vkGetPhysicalDeviceMemoryProperties(physical,&memory_props);
  auto allocate=[&](VkBufferUsageFlags usage,bool host){
    Buffer b={};VkBufferCreateInfo ci={};ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;ci.size=bytes;ci.usage=usage;
    assert(vkCreateBuffer(device,&ci,nullptr,&b.buffer)==VK_SUCCESS);
    VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(device,b.buffer,&requirements);
    uint32_t type=0;while(type<memory_props.memoryTypeCount && (!(requirements.memoryTypeBits&(1u<<type)) ||
      (host && !(memory_props.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))))++type;
    assert(type<memory_props.memoryTypeCount);
    VkMemoryAllocateInfo ai={};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=requirements.size;ai.memoryTypeIndex=type;
    assert(vkAllocateMemory(device,&ai,nullptr,&b.memory)==VK_SUCCESS);
    assert(vkBindBufferMemory(device,b.buffer,b.memory,0)==VK_SUCCESS);
    if(host) assert(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapping)==VK_SUCCESS);
    return b;
  };
  auto source=allocate(VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
  Buffer staging[]={allocate(VK_BUFFER_USAGE_TRANSFER_DST_BIT,true),allocate(VK_BUFFER_USAGE_TRANSFER_DST_BIT,true),allocate(VK_BUFFER_USAGE_TRANSFER_DST_BIT,true)};
  struct Lease {uint64_t id,capacity,submission=0;bool occupied=false;};
  std::vector<std::unique_ptr<Lease>> leases;
  for(unsigned i=0;i<3;++i)leases.push_back(std::make_unique<Lease>(Lease{i+1,bytes}));
  std::vector<VkFence> fences;unsigned waits=0,copies=0;Dispatcher dispatch;
  {
    acv::GeneralReadbackBridge<Memory> bridge(memory,rex::thread::global_critical_region::mutex(),
      [&](auto fn){dispatch.Post(std::move(fn));},[&](uint64_t ticket){
        assert(ticket && ticket<=fences.size());++waits;
        return vkWaitForFences(device,1,&fences[ticket-1],VK_TRUE,5000000000ULL)==VK_SUCCESS;
      },3,2*bytes);
    auto submit=[&](unsigned slot,uint32_t start,uint32_t length,uint32_t pattern){
      assert(bridge.Prepare(length) && bridge.RetireSlot(slot+1));
      auto state=bridge.MakeState({start,length},slot+1);assert(state);
      Memory::PhysicalGpuWriteScope scope(memory,start,length);assert(scope.valid());
      assert(bridge.RegisterLocked(scope.lock(),state));
      memory.EnablePhysicalMemoryAccessCallbacks(start,length,true,true);
      VkCommandBufferAllocateInfo cai={};cai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
      VkCommandBuffer command=VK_NULL_HANDLE;assert(vkAllocateCommandBuffers(device,&cai,&command)==VK_SUCCESS);
      VkCommandBufferBeginInfo begin={};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      assert(vkBeginCommandBuffer(command,&begin)==VK_SUCCESS);
      VkMemoryBarrier source_reuse={};source_reuse.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      source_reuse.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
      source_reuse.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
      vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&source_reuse,0,nullptr,0,nullptr);
      vkCmdFillBuffer(command,source.buffer,0,length,pattern);
      VkBufferMemoryBarrier barrier={};barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
      barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=source.buffer;barrier.size=length;
      vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
      VkBufferCopy region={};region.size=length;vkCmdCopyBuffer(command,source.buffer,staging[slot].buffer,1,&region);
      barrier.buffer=staging[slot].buffer;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
      vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
      assert(vkEndCommandBuffer(command)==VK_SUCCESS);
      VkFenceCreateInfo fci={};fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;VkFence fence=VK_NULL_HANDLE;
      assert(vkCreateFence(device,&fci,nullptr,&fence)==VK_SUCCESS);
      VkSubmitInfo submit={};submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;submit.commandBufferCount=1;submit.pCommandBuffers=&command;
      scope.Unlock();assert(vkQueueSubmit(queue,1,&submit,fence)==VK_SUCCESS);fences.push_back(fence);
      leases[slot]->submission=fences.size();leases[slot]->occupied=true;
      assert(bridge.Commit(state,fences.size(),[&,slot,start,length]{
        VkMappedMemoryRange range={};range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;range.memory=staging[slot].memory;range.size=VK_WHOLE_SIZE;
        if(vkInvalidateMappedMemoryRanges(device,1,&range)!=VK_SUCCESS)return false;
        if(copy_pool)copy_pool->Copy(memory.TranslatePhysical(start),staging[slot].mapping,length);
        else std::memcpy(memory.TranslatePhysical(start),staging[slot].mapping,length);
        ++copies;
        leases[slot]->occupied=false;return true;
      }));
      return state;
    };
    auto first=submit(0,address,bytes,0x11223344);
    auto second=submit(1,address+128,128,0xAABBCCDD);
    assert(!first->ready() && !second->ready() && waits==0 && copies==0);
    // The third copy exceeds the byte cap by128 bytes. Only the oldest full-size
    // copy must publish; the second and third remain staged and protected.
    auto third=submit(2,address+bytes,bytes,0x778899AA);
    assert(first->ready() && !second->ready() && !third->ready() && waits==1 && copies==1);
    // Allocator pressure can retire the oldest fitting occupied slot without
    // publishing the newer third slot. Completion is proven by the real fence.
    Lease* retired=nullptr;
    assert(acv::RetireOldestFittingSlot(leases,bytes,
        [&](uint64_t id){return bridge.RetireSlot(id);},retired));
    assert(retired==leases[1].get() && !retired->occupied && leases[2]->occupied);
    assert(second->ready() && !third->ready() && waits==2 && copies==2);

    // GPU-owned inline publication of overlapping generations: newest exact bytes.
    std::vector<uint32_t> actual(bytes/4);assert(memory.ReadPhysicalCpu(address,actual.data(),bytes));
    for(size_t i=0;i<actual.size();++i)assert(actual[i]==(i>=32 && i<64?0xAABBCCDD:0x11223344));
    assert(waits==2 && copies==2 && bridge.pending()==1 && !third->ready());
    uint32_t third_word=0;
    assert(memory.ReadPhysicalCpu(address+bytes,&third_word,4) && third_word==0x778899AA);
    assert(third->ready() && waits==3 && copies==3 && bridge.pending()==0);
    // A real C/E/A-style guest read fault dispatches to the owner without the memory lock.
    for(unsigned generation=0;generation<8;++generation){
      uint32_t pattern=0xCC000000|generation;auto state=submit(generation%2,address,bytes,pattern);
      uint32_t observed=0;std::thread cpu([&]{observed=*memory.TranslateVirtual<volatile uint32_t*>(guest);});
      dispatch.PumpOne();cpu.join();assert(state->ready() && observed==pattern);
      // Partial CPU write must preserve the newly published neighboring GPU bytes.
      const uint32_t edited=0xDEADBEEF;assert(memory.WritePhysicalCpu(address+4,&edited,4));
      uint32_t pair[2];assert(memory.ReadPhysicalCpu(address,pair,8));assert(pair[0]==pattern && pair[1]==edited);
    }
    // Normal teardown may complete a queued CPU fault before its callback runs.
    auto last=submit(0,address,bytes,0x87654321);
    assert(bridge.Drain() && last->ready());bridge.Close();assert(bridge.CancelAfterDeviceStopped());
    assert(memory.CollectPublishedPhysicalReadbacks());
  }
  assert(vkDeviceWaitIdle(device)==VK_SUCCESS);
  for(auto fence:fences)vkDestroyFence(device,fence,nullptr);
  for(auto& b:staging){vkUnmapMemory(device,b.memory);vkDestroyBuffer(device,b.buffer,nullptr);vkFreeMemory(device,b.memory,nullptr);}
  vkDestroyBuffer(device,source.buffer,nullptr);vkFreeMemory(device,source.memory,nullptr);
  vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
  assert(heap->Release(guest));assert(memory.CancelExplicitPhysicalReadbacksAfterDeviceStopped());
  std::cout<<"PASS real Vulkan/SDK bridge on "<<props.deviceName<<" parallel_copy="<<parallel<<" bytes="<<bytes<<": 12 submitted copies, minimal capacity prefix/oldest fitting slot, overlapping current generations, 8 guest faults, staging reuse, partial CPU writes, normal drain; waits="<<waits<<" copies="<<copies<<"\n";
}
