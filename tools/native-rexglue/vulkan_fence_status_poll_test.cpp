// Real API oracle, no window/game. Shared production completion helper.
#include "vulkan_readback_test_support.h"
#include "submission_fence_wait.h"
using namespace acv::readback_test;
namespace {
constexpr unsigned kCount=32,kBytes=65536,kGuard=4;
void Run(Vulkan& vk,bool status,unsigned trial) {
  std::vector<VkFence> handles(kCount),pending,free;
  std::vector<VkCommandBuffer> commands(kCount);
  for(auto& fence:handles){VkFenceCreateInfo ci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK_CHECK(vkCreateFence(vk.device,&ci,nullptr,&fence));}
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool=vk.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=kCount;
  VK_CHECK(vkAllocateCommandBuffers(vk.device,&ai,commands.data()));
  uint64_t completed=0,poll_calls=0,successes=0,not_ready=0,timeouts=0,blocks=0;
  double poll_us=0;
  auto wait=[&](uint32_t count,const VkFence* fences,uint64_t timeout) {
    Require(timeout!=0,"blocking callback received zero timeout");++blocks;
    return vkWaitForFences(vk.device,count,fences,VK_TRUE,std::min(timeout,uint64_t(5'000'000'000)));
  };
  auto poll=[&](VkFence fence) {
    const auto start=Clock::now();
    const auto result=status?vkGetFenceStatus(vk.device,fence):vkWaitForFences(vk.device,1,&fence,VK_TRUE,0);
    poll_us+=std::chrono::duration<double,std::micro>(Clock::now()-start).count();++poll_calls;
    if(result==VK_SUCCESS)++successes;
    else if(result==VK_NOT_READY)++not_ready;
    else if(result==VK_TIMEOUT)++timeouts;
    else Check(result,"unexpected poll result");
    return result;
  };
  for(unsigned round=0;round<8;++round) {
    Require(round==0 || free==handles,"free-list reset order differs");
    VK_CHECK(vkResetFences(vk.device,kCount,handles.data()));free.clear();pending=handles;
    const auto unsignaled=acv::CompleteSubmissionFences(pending,free,completed,0,false,wait,poll);
    Require(unsignaled.poll_calls==1 && !unsignaled.retired && unsignaled.poll_result==(status?VK_NOT_READY:VK_TIMEOUT),"native unsignaled result lost");
    // Unsubmitted handles are only used above for the negative-status oracle.
    // Production-equivalent list below receives each successful queue submit.
    pending.clear();const auto before=completed;
    for(unsigned i=0;i<kCount;++i) {
      VK_CHECK(vkResetCommandBuffer(commands[i],0));
      VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(commands[i],&bi));
      const VkDeviceSize offset=VkDeviceSize(i)*kBytes;
      // Prefix/tail guards are separate non-overlapping writes, no WAW hazard.
      vkCmdFillBuffer(commands[i],vk.buffers[1].handle,offset,4*kGuard,0xC0FFEE11);
      vkCmdFillBuffer(commands[i],vk.buffers[1].handle,offset+4*kGuard,kBytes-8*kGuard,0xAB000000|(round<<8)|i);
      vkCmdFillBuffer(commands[i],vk.buffers[1].handle,offset+kBytes-4*kGuard,4*kGuard,0xC0FFEE11);
      VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
      barrier.buffer=vk.buffers[1].handle;barrier.offset=offset;barrier.size=kBytes;
      barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
      barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
      vkCmdPipelineBarrier(commands[i],VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
      VK_CHECK(vkEndCommandBuffer(commands[i]));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submit.commandBufferCount=1;submit.pCommandBuffers=&commands[i];
      VK_CHECK(vkQueueSubmit(vk.queue,1,&submit,handles[i]));vk.pending=true;pending.push_back(handles[i]);
    }
    // Deterministic identical signaled inputs avoid measuring different GPU
    // completion races as an API speedup. This setup wait does not retire state.
    VK_CHECK(vkWaitForFences(vk.device,kCount,handles.data(),VK_TRUE,5'000'000'000));
    auto result=acv::CompleteSubmissionFences(pending,free,completed,0,false,wait,poll);
    Require(result.retired==kCount && result.poll_calls==kCount && completed==before+kCount,"poll retirement mismatch");
    Require(pending.empty() && free==handles && !result.device_lost,"free-list order mismatch");
    for(auto fence:handles)VK_CHECK(vkGetFenceStatus(vk.device,fence));
    vk.pending=false;
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=vk.buffers[1].memory;range.size=VK_WHOLE_SIZE;
    VK_CHECK(vkInvalidateMappedMemoryRanges(vk.device,1,&range));
    const auto* words=static_cast<const uint32_t*>(vk.buffers[1].mapping);
    for(unsigned i=0;i<kCount;++i)for(unsigned j=0;j<kBytes/4;++j) {
      const uint32_t expected=(j<kGuard || j>=kBytes/4-kGuard)?0xC0FFEE11:0xAB000000|(round<<8)|i;
      Require(words[i*(kBytes/4)+j]==expected,"GPU bytes or prefix/tail/middle guards differ");
    }
  }
  Require(poll_calls==264 && successes==256 && (status?not_ready:timeouts)==8 && blocks==0,"API counts differ");
  for(auto fence:handles)vkDestroyFence(vk.device,fence,nullptr);
  vkFreeCommandBuffers(vk.device,vk.pool,kCount,commands.data());
  std::cout<<"{\"trial\":"<<trial<<",\"status_poll\":"<<(status?"true":"false")
    <<",\"poll_calls\":"<<poll_calls<<",\"success\":"<<successes<<",\"not_ready\":"<<not_ready<<",\"timeout\":"<<timeouts
    <<",\"poll_api_total_us\":"<<poll_us<<",\"setup_blocking_waits\":8,\"submissions\":256,\"fence_reset_rounds\":8,\"bytes_guards_free_list_ok\":true}\n";
}
}
int main() try {
  const auto deadline=Clock::now()+std::chrono::seconds(60);
  Vulkan vk;vk.Init();vk.Allocate(1,kCount*kBytes,true);
  for(unsigned trial=0;trial<2;++trial)for(unsigned order=0;order<2;++order) {
    Require(Clock::now()<deadline,"deadline exceeded");Run(vk,(trial+order)%2,trial);
  }
  std::cout<<"PASS same-input API AB/BA: 264 polls each, native unsignaled result, 256 signaled GPU fences and exact guarded bytes per mode/trial; reset/reuse; timings are driver microtimings, no FPS claim\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
