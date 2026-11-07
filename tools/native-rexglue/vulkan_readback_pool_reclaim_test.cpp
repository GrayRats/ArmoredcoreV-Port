// Same-input mixed-size GPU-write/readback oracle using the production bridge,
// provider registry and slot selection/reclamation policy. No game or window.
// clang++ -std=c++23 -O3 -march=x86-64-v2 -pthread FILE -lvulkan -o OUT
#include "vulkan_readback_test_support.h"
#include "general_readback_bridge.h"
#include "physical_readback_providers.h"
#include "readback_pool_selection.h"
#include <cstring>
#include <memory>
using namespace acv::readback_test;
namespace {
constexpr uint32_t kLarge=7249920, kSmall=3768320, kGuestSize=8<<20;
constexpr uint64_t kBudget=64ULL<<20;
struct Memory {
  using PhysicalReadbackProvider=acv::PhysicalReadbackProviders::Provider;
  std::recursive_mutex mutex;
  acv::PhysicalReadbackProviders providers{mutex};
  std::vector<uint8_t> data=std::vector<uint8_t>(kGuestSize+128,0xCE);
  std::vector<uint8_t> oracle=data;
  bool RegisterExplicitPhysicalReadbackLocked(std::unique_lock<std::recursive_mutex>& lock,
      PhysicalReadbackProvider provider) {return providers.RegisterLocked(lock,std::move(provider));}
  bool CollectPublishedPhysicalReadbacks() {
    std::unique_lock lock(mutex);return providers.CollectPublished(lock);
  }
  void CheckCpu() {
    std::unique_lock lock(mutex);
    Require(providers.Synchronize(lock,{64,kGuestSize}),"CPU synchronization failed");
    Require(data==oracle,"current-generation or guard mismatch");
  }
  void CpuWrite(uint32_t offset) {
    std::unique_lock lock(mutex);
    Require(providers.Synchronize(lock,{offset,4}),"partial CPU write synchronization failed");
    uint32_t value=0xFEEDC0DE;std::memcpy(data.data()+offset,&value,4);std::memcpy(oracle.data()+offset,&value,4);
  }
};
struct Slot {
  uint64_t id{},capacity{},allocation{},submission{};bool occupied=false;
  VkBuffer buffer{};VkDeviceMemory memory{};void* mapping{};
};
struct Counts {uint64_t fitting=0,admission=0,cpu=0,other=0,reclaimed=0,published=0,max_pending=0,peak_bytes=0;};
Counts Run(bool enabled,unsigned trial) {
  Vulkan vk;vk.Init();vk.Allocate(0,kGuestSize,false);
  Memory memory;std::vector<std::unique_ptr<Slot>> slots;
  std::vector<VkFence> fences;uint64_t allocated=0,next_id=1;Counts counts;
  enum class Reason {Other,Admission,Fitting};Reason reason=Reason::Other;
  using Bridge=acv::GeneralReadbackBridge<Memory>;
  Bridge* active=nullptr;
  Bridge bridge(memory,memory.mutex,[](auto callback){callback();},[&](uint64_t ticket) {
    Require(ticket && ticket<=fences.size(),"invalid fence ticket");
    if(active->servicing_cpu_access())++counts.cpu;
    else if(reason==Reason::Admission)++counts.admission;
    else if(reason==Reason::Fitting)++counts.fitting;
    else ++counts.other;
    VK_CHECK(vkWaitForFences(vk.device,1,&fences[ticket-1],VK_TRUE,5'000'000'000ULL));
    return true;
  },64,kBudget);
  active=&bridge;
  auto release=[&](Slot& slot) noexcept {
    if(slot.mapping)vkUnmapMemory(vk.device,slot.memory);
    if(slot.buffer)vkDestroyBuffer(vk.device,slot.buffer,nullptr);
    if(slot.memory)vkFreeMemory(vk.device,slot.memory,nullptr);
    slot.mapping=nullptr;slot.buffer={};slot.memory={};
  };
  auto make_buffer=[&](uint32_t bytes) {
    auto fresh=std::make_unique<Slot>();fresh->capacity=bytes;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=bytes;
    ci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vk.device,&ci,nullptr,&fresh->buffer));return fresh;
  };
  auto bind=[&](Slot& fresh) {
    VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(vk.device,fresh.buffer,&requirements);
    uint32_t type=UINT32_MAX;
    for(unsigned pass=0;pass<2 && type==UINT32_MAX;++pass) {
      const VkMemoryPropertyFlags flags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|(pass?0:VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
      for(uint32_t i=0;i<vk.memory_properties.memoryTypeCount;++i)
        if((requirements.memoryTypeBits&(1u<<i)) && (vk.memory_properties.memoryTypes[i].propertyFlags&flags)==flags){type=i;break;}
    }
    Require(type!=UINT32_MAX,"no readback memory type");
    Require(allocated+requirements.size<=kBudget,"allocation exceeds unchanged64MiB budget");
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.buffer=fresh.buffer;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&dedicated;
    ai.allocationSize=requirements.size;ai.memoryTypeIndex=type;
    VK_CHECK(vkAllocateMemory(vk.device,&ai,nullptr,&fresh.memory));
    VK_CHECK(vkBindBufferMemory(vk.device,fresh.buffer,fresh.memory,0));
    VK_CHECK(vkMapMemory(vk.device,fresh.memory,0,VK_WHOLE_SIZE,0,&fresh.mapping));
    fresh.allocation=requirements.size;allocated+=fresh.allocation;fresh.id=next_id++;
    counts.peak_bytes=std::max(counts.peak_bytes,allocated);
  };
  const auto deadline=Clock::now()+std::chrono::seconds(30);
  auto submit=[&](uint32_t length,uint32_t address,uint32_t pattern) {
    Require(Clock::now()<deadline,"30s oracle deadline exceeded");
    reason=Reason::Admission;Require(bridge.Prepare(length),"bridge admission failed");reason=Reason::Other;
    Slot* slot=nullptr;
    for(auto& candidate:slots)if(!candidate->occupied && candidate->capacity>=length){slot=candidate.get();break;}
    std::unique_ptr<Slot> fresh;
    if(!slot && enabled && (slots.size()>=64 || allocated+length>kBudget)) {
      fresh=make_buffer(length);
      VkMemoryRequirements req{};vkGetBufferMemoryRequirements(vk.device,fresh->buffer,&req);
      const size_t before=slots.size();
      const auto result=acv::ReclaimIdleUndersizedSlots(slots,allocated,length,req.size,64,kBudget,
          [&](uint64_t id){return bridge.RetireSlot(id);},release);
      Require(result!=acv::IdleReclaimResult::kFailed,"idle reclamation failed");
      counts.reclaimed+=before-slots.size();
      if(result!=acv::IdleReclaimResult::kReclaimed){release(*fresh);fresh.reset();}
    }
    if(!slot && !fresh && (slots.size()>=64 || allocated+length>kBudget)) {
      reason=Reason::Fitting;
      Require(acv::RetireOldestFittingSlot(slots,length,[&](uint64_t id){return bridge.RetireSlot(id);},slot),"fitting retirement failed");
      reason=Reason::Other;
      if(!slot) {
        Require(bridge.Drain(),"pool drain failed");
        for(auto& candidate:slots)if(!candidate->occupied && candidate->capacity>=length){slot=candidate.get();break;}
        if(!slot){for(auto& candidate:slots){Require(!candidate->occupied,"drain left occupied slot");release(*candidate);}slots.clear();allocated=0;}
      }
    }
    if(!slot) {if(!fresh)fresh=make_buffer(length);bind(*fresh);slot=fresh.get();slots.push_back(std::move(fresh));}
    Require(slots.size()<=64 && allocated<=kBudget,"pool bounds violated");
    Require(bridge.RetireSlot(slot->id),"slot lease not retired");
    auto state=bridge.MakeState({address,length},slot->id);Require(bool(state),"state creation failed");
    {std::unique_lock lock(memory.mutex);Require(bridge.RegisterLocked(lock,state),"provider registration failed");}
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool=vk.pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
    VkCommandBuffer command{};VK_CHECK(vkAllocateCommandBuffers(vk.device,&cai,&command));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(command,&begin));
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=vk.buffers[0].handle;barrier.size=VK_WHOLE_SIZE;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    vkCmdFillBuffer(command,vk.buffers[0].handle,0,length,pattern);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    VkBufferCopy copy{0,0,length};vkCmdCopyBuffer(command,vk.buffers[0].handle,slot->buffer,1,&copy);
    barrier.buffer=slot->buffer;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    VK_CHECK(vkEndCommandBuffer(command));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence{};VK_CHECK(vkCreateFence(vk.device,&fci,nullptr,&fence));
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command;
    VK_CHECK(vkQueueSubmit(vk.queue,1,&info,fence));vk.pending=true;fences.push_back(fence);
    slot->occupied=true;slot->submission=fences.size();
    Require(bridge.Commit(state,slot->submission,[&,slot,address,length,pattern] {
      VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=slot->memory;range.size=VK_WHOLE_SIZE;
      VK_CHECK(vkInvalidateMappedMemoryRanges(vk.device,1,&range));
      auto* words=static_cast<const uint32_t*>(slot->mapping);
      for(size_t i=0;i<length/4;++i)Require(words[i]==pattern,"stale/corrupt GPU staging generation");
      std::memcpy(memory.data.data()+address,slot->mapping,length);
      slot->occupied=false;++counts.published;return true;
    }),"bridge commit failed");
    for(size_t i=0;i<length;i+=4)std::memcpy(memory.oracle.data()+address+i,&pattern,4);
    counts.max_pending=std::max(counts.max_pending,uint64_t(bridge.pending()));
  };
  // Warm-up deliberately leaves a historical mixed-size pool: one large slot,
  // fifteen smaller completed slots. This reproduces the structural allocation
  // pressure case; it is not a replay of unrecorded gameplay allocation history.
  submit(kLarge,64,0x11000001);
  for(unsigned i=0;i<15;++i)submit(kSmall,64,0x22000000+i);
  Require(bridge.Drain(),"warmup drain failed");memory.CheckCpu();
  Require(slots.size()==16 && allocated<=kBudget,"unexpected warmup pool geometry");
  counts={};counts.peak_bytes=allocated;
  const auto start=Clock::now();
  for(unsigned i=0;i<32;++i)submit(i%2 ? 6u<<20 : kLarge,64+(i%3)*64,0x33000000+i);
  // CPU demand must publish all overlapping predecessors through the newest.
  memory.CheckCpu();
  const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
  const Counts measured=counts;
  Require(measured.published==32,"publication missing or duplicated");
  memory.CpuWrite(256);
  submit(4096,4096,0x55667788); // Disjoint from the partial CPU write.
  memory.CheckCpu();Require(bridge.Drain(),"final drain failed");
  Require(bridge.pending()==0 && memory.providers.pending()==0,"pending providers at teardown");
  bridge.Close();Require(bridge.CancelAfterDeviceStopped(),"bridge close failed");
  VK_CHECK(vkWaitForFences(vk.device,1,&fences.back(),VK_TRUE,5'000'000'000ULL));vk.pending=false;
  for(auto& candidate:slots){Require(!candidate->occupied,"occupied slot at destroy");release(*candidate);}slots.clear();
  for(auto fence:fences)vkDestroyFence(vk.device,fence,nullptr);
  Require(memory.providers.CancelAfterDeviceStopped(),"provider cancel failed");
  std::cout<<"{\"trial\":"<<trial<<",\"reclaim_idle\":"<<(enabled?"true":"false")
    <<",\"fitting_waits\":"<<measured.fitting<<",\"admission_waits\":"<<measured.admission
    <<",\"cpu_waits\":"<<measured.cpu<<",\"other_waits\":"<<measured.other
    <<",\"reclaimed_slots\":"<<measured.reclaimed<<",\"max_pending\":"<<measured.max_pending
    <<",\"peak_allocated_bytes\":"<<measured.peak_bytes<<",\"published\":"<<measured.published
    <<",\"elapsed_ms\":"<<ms<<",\"exact_bytes_guards_cpu_write\":true}\n";
  return measured;
}
}
int main() try {
  for(unsigned trial=0;trial<2;++trial) {
    Counts result[2];for(unsigned order=0;order<2;++order){const bool enabled=(trial+order)%2;result[enabled]=Run(enabled,trial);}
    Require(result[1].fitting<result[0].fitting && result[1].max_pending>result[0].max_pending,"reclaim did not reduce structural fitting pressure");
    Require(result[1].reclaimed>0 && result[1].peak_bytes<=kBudget,"reclaim bounds/effect mismatch");
    Require(result[0].published==result[1].published,"policy publication mismatch");
  }
  std::cout<<"PASS mixed-size same-input GPU oracle AB/BA: exact generations, CPU read/partial write, guards, publication count, bounded memory and fewer forced fitting waits; no FPS claim\n";
  return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
