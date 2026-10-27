// Same-input Vulkan transfer-pressure comparison of actual old/new queue code.
// No game frames, screenshot, original assets, or gameplay FPS measurement.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#define acv acv_baseline
#include "acv_readback_baseline.h"
#undef acv
#include "../rexglue-sdk/src/graphics/vulkan/deferred_readback_queue.h"
using Clock=std::chrono::steady_clock;
static double ms(Clock::duration value){return std::chrono::duration<double,std::milli>(value).count();}
constexpr uint32_t bytes=4u<<20,slot_count=16,copies=512,warmup=64;
struct Device {
 VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;
 VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;uint32_t family=0;
 VkPhysicalDeviceProperties properties{};VkPhysicalDeviceMemoryProperties memory{};
 Device(){
  VkInstanceCreateInfo ici={};ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  assert(vkCreateInstance(&ici,nullptr,&instance)==VK_SUCCESS);
  uint32_t count=0;assert(vkEnumeratePhysicalDevices(instance,&count,nullptr)==VK_SUCCESS && count);
  std::vector<VkPhysicalDevice> devices(count);assert(vkEnumeratePhysicalDevices(instance,&count,devices.data())==VK_SUCCESS);
  int best=-1;
  for(auto candidate:devices){VkPhysicalDeviceProperties p;vkGetPhysicalDeviceProperties(candidate,&p);
   int score=p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?3:p.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?2:p.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU?1:0;
   if(score>best){best=score;physical=candidate;properties=p;}}
  assert(best>0); // Software rendering cannot answer this GPU-pressure question.
  vkGetPhysicalDeviceMemoryProperties(physical,&memory);
  vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
  std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
  while(family<count && !(families[family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)))++family;
  assert(family<count);float priority=1;
  VkDeviceQueueCreateInfo qci={};qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
  VkDeviceCreateInfo dci={};dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
  assert(vkCreateDevice(physical,&dci,nullptr,&device)==VK_SUCCESS);vkGetDeviceQueue(device,family,0,&queue);
 }
 ~Device(){assert(vkDeviceWaitIdle(device)==VK_SUCCESS);vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);}
};
struct Buffer {
 Device& d;VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;
 Buffer(Device& device,VkBufferUsageFlags usage,bool host,VkDeviceSize size=bytes):d(device){
  VkBufferCreateInfo ci={};ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;ci.size=size;ci.usage=usage;
  assert(vkCreateBuffer(d.device,&ci,nullptr,&buffer)==VK_SUCCESS);
  VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(d.device,buffer,&requirements);
  uint32_t selected=UINT32_MAX;int best=-1;
  for(uint32_t i=0;i<d.memory.memoryTypeCount;++i){
   auto flags=d.memory.memoryTypes[i].propertyFlags;
   if(!(requirements.memoryTypeBits&(1u<<i)) || (host && !(flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)))continue;
   int score=host?int(bool(flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)):int(bool(flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
   if(score>best){best=score;selected=i;}}
  assert(selected!=UINT32_MAX);
  VkMemoryAllocateInfo ai={};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=requirements.size;ai.memoryTypeIndex=selected;
  assert(vkAllocateMemory(d.device,&ai,nullptr,&memory)==VK_SUCCESS);
  assert(vkBindBufferMemory(d.device,buffer,memory,0)==VK_SUCCESS);
  if(host)assert(vkMapMemory(d.device,memory,0,VK_WHOLE_SIZE,0,&mapped)==VK_SUCCESS);
 }
 ~Buffer(){if(mapped)vkUnmapMemory(d.device,memory);vkDestroyBuffer(d.device,buffer,nullptr);vkFreeMemory(d.device,memory,nullptr);}
};
struct Slot {
 Device& d;Buffer staging;VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
 uint64_t ticket=0;bool occupied=false;uint64_t id=0,capacity=bytes,submission=0;
 explicit Slot(Device& device,uint64_t size=bytes,uint64_t ident=0):d(device),staging(device,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true,size),id(ident),capacity(size){
  VkCommandPoolCreateInfo pci={};pci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pci.queueFamilyIndex=d.family;
  assert(vkCreateCommandPool(d.device,&pci,nullptr,&pool)==VK_SUCCESS);
  VkCommandBufferAllocateInfo cai={};cai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
  assert(vkAllocateCommandBuffers(d.device,&cai,&command)==VK_SUCCESS);
  VkFenceCreateInfo fci={};fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;assert(vkCreateFence(d.device,&fci,nullptr,&fence)==VK_SUCCESS);
 }
 ~Slot(){vkDestroyFence(d.device,fence,nullptr);vkDestroyCommandPool(d.device,pool,nullptr);}
};
struct Stats {double elapsed=0,wait_ms=0,publish_ms=0;uint64_t waits=0,published=0,peak=0;};
template<class Queue> Stats run(Device& d,const char* policy,unsigned trial){
 Buffer source(d,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
 std::vector<std::unique_ptr<Slot>> slots;for(unsigned i=0;i<slot_count;++i)slots.push_back(std::make_unique<Slot>(d));
 std::vector<uint32_t> destination(bytes/4);Stats stats;
 Queue queue([&](uint64_t ticket){
  auto it=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s->ticket==ticket;});assert(it!=slots.end());
  auto start=Clock::now();VkResult result=vkWaitForFences(d.device,1,&(*it)->fence,VK_TRUE,5000000000ULL);
  stats.wait_ms+=ms(Clock::now()-start);++stats.waits;return result==VK_SUCCESS;
 },64,uint64_t(bytes)*slot_count);
 uint64_t ticket=0;
 auto submit=[&]{
  assert(queue.PrepareForEnqueue(bytes));
  auto it=std::find_if(slots.begin(),slots.end(),[](const auto& s){return !s->occupied;});assert(it!=slots.end());
  Slot* slot=it->get();uint64_t id=uint64_t(it-slots.begin())+1;
  assert(queue.BeforeSlotReuse(id) && !slot->occupied);
  assert(vkResetFences(d.device,1,&slot->fence)==VK_SUCCESS);
  assert(vkResetCommandPool(d.device,slot->pool,0)==VK_SUCCESS);
  VkCommandBufferBeginInfo begin={};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  assert(vkBeginCommandBuffer(slot->command,&begin)==VK_SUCCESS);
  VkMemoryBarrier reuse={};reuse.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;reuse.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;reuse.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
  const uint32_t pattern=0x70000000u|uint32_t(++ticket);
  // Reproducible device work before transfer; identical for both policies.
  for(unsigned fill=0;fill<8;++fill){
   vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&reuse,0,nullptr,0,nullptr);
   vkCmdFillBuffer(slot->command,source.buffer,0,bytes,pattern);
  }
  VkBufferMemoryBarrier barrier={};barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=source.buffer;barrier.size=bytes;
  vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  VkBufferCopy copy={};copy.size=bytes;vkCmdCopyBuffer(slot->command,source.buffer,slot->staging.buffer,1,&copy);
  barrier.buffer=slot->staging.buffer;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  assert(vkEndCommandBuffer(slot->command)==VK_SUCCESS);
  VkSubmitInfo si={};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&slot->command;
  assert(vkQueueSubmit(d.queue,1,&si,slot->fence)==VK_SUCCESS);slot->ticket=ticket;slot->occupied=true;
  assert(queue.Enqueue({{0,bytes},id,ticket,[&,slot,pattern]{
   auto start=Clock::now();VkMappedMemoryRange range={};range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;range.memory=slot->staging.memory;range.size=VK_WHOLE_SIZE;
   if(vkInvalidateMappedMemoryRanges(d.device,1,&range)!=VK_SUCCESS)return false;
   std::memcpy(destination.data(),slot->staging.mapped,bytes);
   assert(destination.front()==pattern && destination[destination.size()/2]==pattern && destination.back()==pattern);
   slot->occupied=false;stats.publish_ms+=ms(Clock::now()-start);++stats.published;return true;
  }}));stats.peak=std::max(stats.peak,uint64_t(queue.pending()));
  assert(queue.pending_bytes()<=uint64_t(bytes)*slot_count);
 };
 for(unsigned i=0;i<warmup;++i)submit();assert(queue.Drain());stats={};
 auto start=Clock::now();for(unsigned i=0;i<copies;++i)submit();assert(queue.Drain());stats.elapsed=ms(Clock::now()-start);
 const uint32_t expected=0x70000000u|uint32_t(ticket);assert(std::all_of(destination.begin(),destination.end(),[=](uint32_t value){return value==expected;}));
 assert(stats.published==copies && queue.pending()==0);assert(vkDeviceWaitIdle(d.device)==VK_SUCCESS);
 std::cout<<"{\"scope\":\"synthetic same-input Vulkan transfer pressure; not gameplay FPS\",\"policy\":\""<<policy
   <<"\",\"trial\":"<<trial<<",\"gpu\":\""<<d.properties.deviceName<<"\",\"bytes_per_copy\":"<<bytes
   <<",\"copies\":"<<copies<<",\"elapsed_ms\":"<<stats.elapsed<<",\"wait_ms\":"<<stats.wait_ms
   <<",\"wait_calls\":"<<stats.waits<<",\"publish_ms\":"<<stats.publish_ms<<",\"peak_jobs\":"<<stats.peak<<"}\n";
 return stats;
}
#ifndef ACV_PRESSURE_BENCHMARK_NO_MAIN
int main(){Device device;for(unsigned trial=0;trial<3;++trial){
 if(trial%2){run<acv::DeferredReadbackQueue>(device,"minimal_prefix",trial);run<acv_baseline::DeferredReadbackQueue>(device,"drain_all",trial);}
 else{run<acv_baseline::DeferredReadbackQueue>(device,"drain_all",trial);run<acv::DeferredReadbackQueue>(device,"minimal_prefix",trial);}
}}

#endif
