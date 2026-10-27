// Same captured queue algorithm for both trials; only allocation-pressure
// retirement differs. Fixed mixed-size Vulkan pool, no game/FPS measurement.
#define ACV_PRESSURE_BENCHMARK_NO_MAIN
#include "vulkan_readback_pressure_benchmark.cpp"
#include "../rexglue-sdk/src/graphics/vulkan/readback_pool_selection.h"
template<bool oldest> Stats mixed_run(Device& d,unsigned trial){
 Buffer source(d,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
 std::vector<std::unique_ptr<Slot>> slots;
 for(unsigned i=0;i<slot_count;++i)slots.push_back(std::make_unique<Slot>(d,i<8?(1u<<20):bytes,i+1));
 constexpr uint64_t budget=40ULL<<20;
 std::vector<uint32_t> destination(bytes/4);Stats stats;
 uint64_t full_drains=0,fitting_retires=0,total_bytes=0;
 acv::DeferredReadbackQueue queue([&](uint64_t ticket){
  auto it=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s->ticket==ticket;});assert(it!=slots.end());
  auto start=Clock::now();VkResult result=vkWaitForFences(d.device,1,&(*it)->fence,VK_TRUE,5000000000ULL);
  stats.wait_ms+=ms(Clock::now()-start);++stats.waits;return result==VK_SUCCESS;
 },64,budget);
 uint64_t ticket=0;uint32_t last_length=0;
 auto submit=[&](unsigned index){
  uint32_t length=index%32<16?(1u<<20):bytes;
  assert(queue.PrepareForEnqueue(length));
  Slot* slot=nullptr;
  for(auto& candidate:slots)if(!candidate->occupied && candidate->capacity>=length){slot=candidate.get();break;}
  if(!slot){
   if constexpr(oldest){
    assert(acv::RetireOldestFittingSlot(slots,length,[&](uint64_t id){return queue.BeforeSlotReuse(id);},slot));
    assert(slot && !slot->occupied);++fitting_retires;
   }else{
    assert(queue.Drain());++full_drains;
    for(auto& candidate:slots)if(!candidate->occupied && candidate->capacity>=length){slot=candidate.get();break;}
   }
  }
  assert(slot && queue.BeforeSlotReuse(slot->id) && !slot->occupied);
  assert(vkResetFences(d.device,1,&slot->fence)==VK_SUCCESS);assert(vkResetCommandPool(d.device,slot->pool,0)==VK_SUCCESS);
  VkCommandBufferBeginInfo begin={};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  assert(vkBeginCommandBuffer(slot->command,&begin)==VK_SUCCESS);
  VkMemoryBarrier reuse={};reuse.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;reuse.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;reuse.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
  const uint32_t pattern=0x70000000u|uint32_t(++ticket);
  for(unsigned fill=0;fill<8;++fill){
   vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&reuse,0,nullptr,0,nullptr);
   vkCmdFillBuffer(slot->command,source.buffer,0,length,pattern);
  }
  VkBufferMemoryBarrier barrier={};barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=source.buffer;barrier.size=length;
  vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  VkBufferCopy copy={};copy.size=length;vkCmdCopyBuffer(slot->command,source.buffer,slot->staging.buffer,1,&copy);
  barrier.buffer=slot->staging.buffer;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(slot->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  assert(vkEndCommandBuffer(slot->command)==VK_SUCCESS);
  VkSubmitInfo si={};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&slot->command;
  assert(vkQueueSubmit(d.queue,1,&si,slot->fence)==VK_SUCCESS);slot->ticket=slot->submission=ticket;slot->occupied=true;
  assert(queue.Enqueue({{0,length},slot->id,ticket,[&,slot,pattern,length]{
   auto start=Clock::now();VkMappedMemoryRange range={};range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;range.memory=slot->staging.memory;range.size=VK_WHOLE_SIZE;
   if(vkInvalidateMappedMemoryRanges(d.device,1,&range)!=VK_SUCCESS)return false;
   std::memcpy(destination.data(),slot->staging.mapped,length);
   assert(destination[0]==pattern && destination[length/8]==pattern && destination[length/4-1]==pattern);
   slot->occupied=false;stats.publish_ms+=ms(Clock::now()-start);++stats.published;return true;
  }}));stats.peak=std::max(stats.peak,uint64_t(queue.pending()));total_bytes+=length;last_length=length;
  assert(queue.pending_bytes()<=budget);
 };
 for(unsigned i=0;i<warmup;++i)submit(i);assert(queue.Drain());stats={};full_drains=fitting_retires=total_bytes=0;
 auto start=Clock::now();for(unsigned i=0;i<copies;++i)submit(i);assert(queue.Drain());stats.elapsed=ms(Clock::now()-start);
 const uint32_t expected=0x70000000u|uint32_t(ticket);assert(std::all_of(destination.begin(),destination.begin()+last_length/4,[=](uint32_t v){return v==expected;}));
 assert(stats.published==copies && queue.pending()==0);if constexpr(oldest)assert(full_drains==0 && fitting_retires>0);
 assert(vkDeviceWaitIdle(d.device)==VK_SUCCESS);
 std::cout<<"{\"scope\":\"synthetic same-input mixed-size Vulkan pool; not gameplay FPS\",\"policy\":\""
   <<(oldest?"oldest_fitting":"drain_all_allocator")<<"\",\"trial\":"<<trial<<",\"gpu\":\""<<d.properties.deviceName
   <<"\",\"copies\":"<<copies<<",\"request_bytes_total\":"<<total_bytes<<",\"pool_bytes\":"<<budget
   <<",\"elapsed_ms\":"<<stats.elapsed<<",\"wait_ms\":"<<stats.wait_ms<<",\"wait_calls\":"<<stats.waits
   <<",\"publish_ms\":"<<stats.publish_ms<<",\"peak_jobs\":"<<stats.peak<<",\"full_drains\":"<<full_drains<<",\"fitting_retires\":"<<fitting_retires<<"}\n";
 return stats;
}
int main(){Device d;for(unsigned trial=0;trial<3;++trial){
 if(trial%2){mixed_run<true>(d,trial);mixed_run<false>(d,trial);}else{mixed_run<false>(d,trial);mixed_run<true>(d,trial);}
}}
