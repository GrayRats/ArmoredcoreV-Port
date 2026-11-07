// Real same-VkQueue multi-fence oracle for the SDK's shared completion helper.
// No game/window. Compile with -std=c++23 -O3 -march=x86-64-v2 -lvulkan.
#include "vulkan_readback_test_support.h"
#include "submission_fence_wait.h"
#include <cstring>
using namespace acv::readback_test;
namespace {
constexpr uint32_t kCount=32, kBytes=65536;
struct Metrics {
  uint64_t blocking_calls=0,requested=0,passed=0,polls=0,checked_statuses=0;
  std::vector<double> first_wait_us;
};
double Median(std::vector<double> values){std::sort(values.begin(),values.end());return values[values.size()/2];}
Metrics Run(Vulkan& vk,bool latest,unsigned trial,uint32_t prefix) {
  std::vector<VkFence> handles(kCount),in_flight,free;
  std::vector<VkCommandBuffer> commands(kCount);
  for(auto& fence:handles){VkFenceCreateInfo ci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK_CHECK(vkCreateFence(vk.device,&ci,nullptr,&fence));}
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool=vk.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=kCount;
  VK_CHECK(vkAllocateCommandBuffers(vk.device,&ai,commands.data()));
  Metrics metrics;uint64_t completed=0;
  for(unsigned round=0;round<8;++round) {
    const uint64_t previous=completed;
    Require(round==0 || free==handles,"free-list ordering/reuse changed");
    VK_CHECK(vkResetFences(vk.device,kCount,handles.data()));
    free.clear();in_flight.clear();
    std::vector<uint32_t> patterns;
    for(unsigned i=0;i<kCount;++i) {
      const uint32_t pattern=0xAB000000|(prefix<<16)|(trial<<12)|(round<<6)|i;
      patterns.push_back(pattern);
      VK_CHECK(vkResetCommandBuffer(commands[i],0));
      VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(commands[i],&begin));
      vkCmdFillBuffer(commands[i],vk.buffers[1].handle,VkDeviceSize(i)*kBytes,kBytes,pattern);
      VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
      barrier.buffer=vk.buffers[1].handle;barrier.offset=VkDeviceSize(i)*kBytes;barrier.size=kBytes;
      barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
      barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
      vkCmdPipelineBarrier(commands[i],VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
      VK_CHECK(vkEndCommandBuffer(commands[i]));
      VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&commands[i];
      VK_CHECK(vkQueueSubmit(vk.queue,1,&submit,handles[i]));vk.pending=true;
      in_flight.push_back(handles[i]); // Exactly the SDK successful-submit boundary.
    }
    auto wait=[&](uint32_t count,const VkFence* fences,uint64_t timeout) {
      const auto start=Clock::now();
      // Same helper arguments, but every GPU-oracle wait is bounded to5seconds.
      const VkResult status=vkWaitForFences(vk.device,count,fences,VK_TRUE,std::min(timeout,uint64_t(5'000'000'000)));
      const double elapsed=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
      if(round>=2){if(timeout){++metrics.blocking_calls;metrics.passed+=count;}else ++metrics.polls;}
      if(timeout && round>=2 && completed==previous)metrics.first_wait_us.push_back(elapsed);
      return status;
    };
    const auto first=acv::CompleteSubmissionFences(in_flight,free,completed,previous+prefix,latest,wait);
    Require(first.valid_range && !first.device_lost && first.blocking_result==VK_SUCCESS,"required fence wait failed");
    Require(first.poll_result==VK_SUCCESS || first.poll_result==VK_TIMEOUT,"unexpected fence poll result");
    Require(first.requested_prefix==prefix && first.blocking_fences==(latest?1:prefix),"wrong required fence selection");
    Require(first.retired>=prefix && first.retired<=kCount && completed==previous+first.retired,"wrong completed prefix");
    Require(free==std::vector<VkFence>(handles.begin(),handles.begin()+first.retired),"wrong reclaimed fence prefix");
    Require(in_flight==std::vector<VkFence>(handles.begin()+first.retired,handles.end()),"future fence removed incorrectly");
    // Directly check EVERY earlier fence signal after waiting the newest one.
    // Later fence statuses may naturally advance while the CPU verifies this.
    for(unsigned i=0;i<kCount;++i) {
      const VkResult status=vkGetFenceStatus(vk.device,handles[i]);
      if(i<first.retired)Require(status==VK_SUCCESS,"retired earlier fence not signaled");
      else Require(status==VK_SUCCESS || status==VK_NOT_READY,"unexpected later fence status");
      if(round>=2)++metrics.checked_statuses;
    }
    if(round>=2)metrics.requested+=first.requested_prefix;
    if(!in_flight.empty()) {
      const auto rest=acv::CompleteSubmissionFences(in_flight,free,completed,previous+kCount,latest,wait);
      Require(rest.valid_range && !rest.device_lost && rest.blocking_result==VK_SUCCESS && rest.poll_result==VK_SUCCESS,"remaining prefix completion failed");
      if(round>=2)metrics.requested+=rest.requested_prefix;
    }
    Require(in_flight.empty() && free==handles && completed==previous+kCount,"retirement or free-list mismatch");
    for(auto fence:handles)VK_CHECK(vkGetFenceStatus(vk.device,fence));
    vk.pending=false;
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=vk.buffers[1].memory;range.size=VK_WHOLE_SIZE;
    VK_CHECK(vkInvalidateMappedMemoryRanges(vk.device,1,&range));
    const auto* words=static_cast<const uint32_t*>(vk.buffers[1].mapping);
    for(unsigned i=0;i<kCount;++i)for(unsigned word=0;word<kBytes/4;++word)
      Require(words[i*(kBytes/4)+word]==patterns[i],"GPU-written mapped byte oracle mismatch");
  }
  for(auto fence:handles)vkDestroyFence(vk.device,fence,nullptr);
  vkFreeCommandBuffers(vk.device,vk.pool,kCount,commands.data());
  std::cout<<"{\"trial\":"<<trial<<",\"latest\":"<<(latest?"true":"false")<<",\"requested_prefix\":"<<prefix
    <<",\"measured_rounds\":6,\"blocking_wait_calls\":"<<metrics.blocking_calls
    <<",\"original_prefix_fences\":"<<metrics.requested<<",\"passed_fences\":"<<metrics.passed
    <<",\"poll_calls\":"<<metrics.polls<<",\"checked_first_statuses\":"<<metrics.checked_statuses
    <<",\"first_blocking_median_us\":"<<Median(metrics.first_wait_us)<<",\"bytes_and_reuse_ok\":true}\n";
  return metrics;
}
}
int main() try {
  const auto deadline=Clock::now()+std::chrono::seconds(90);
  Vulkan vk;vk.Init();vk.Allocate(1,kCount*kBytes,true);
  for(unsigned trial=0;trial<2;++trial)for(uint32_t prefix:{1u,2u,8u,32u}) {
    Metrics result[2];
    for(unsigned order=0;order<2;++order){const bool latest=(trial+order)%2;Require(Clock::now()<deadline,"90s deadline exceeded");result[latest]=Run(vk,latest,trial,prefix);}
    Require(result[0].passed==result[0].requested,"baseline did not pass original prefix");
    Require(result[1].passed==result[1].blocking_calls,"latest mode passed more than one fence");
    if(prefix>1)Require(result[1].passed<result[1].requested,"latest mode did not reduce fence arguments");
  }
  std::cout<<"PASS real same-queue fence AB/BA: all earlier fence statuses, exact GPU-written bytes, shared prefix/free-list retirement and repeated reset/reuse; no FPS claim\n";
  return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
