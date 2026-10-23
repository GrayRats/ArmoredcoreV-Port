// Host contract test; mock Vulkan dispatch, no claim of driver/GPU validation.
#include "readback_reference.hpp"
#include <cassert>
#include <array>
#include <vector>
#include <string>
static std::vector<std::string> calls;
static VkResult submitResult=VK_SUCCESS,waitResult=VK_SUCCESS,invalidateResult=VK_SUCCESS;
extern "C" {
void vkCmdPipelineBarrier(VkCommandBuffer,VkPipelineStageFlags src,VkPipelineStageFlags dst,
 VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t count,const VkBufferMemoryBarrier* b,
 uint32_t,const VkImageMemoryBarrier*) {
 assert(count==1 && b->srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED);
 if(calls.empty()) {
  assert(src==VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT && dst==VK_PIPELINE_STAGE_TRANSFER_BIT);
  assert(b->srcAccessMask==VK_ACCESS_SHADER_WRITE_BIT && b->dstAccessMask==VK_ACCESS_TRANSFER_READ_BIT);
  calls.push_back("gpu-visible");
 } else {
  assert(src==VK_PIPELINE_STAGE_TRANSFER_BIT && dst==VK_PIPELINE_STAGE_HOST_BIT);
  assert(b->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT && b->dstAccessMask==VK_ACCESS_HOST_READ_BIT);
  calls.push_back("host-visible");
 }
}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer,VkBuffer,uint32_t n,const VkBufferCopy* c) {
 assert(n==1 && c->size==16);calls.push_back("copy");
}
VkResult vkEndCommandBuffer(VkCommandBuffer){calls.push_back("end");return VK_SUCCESS;}
VkResult vkQueueSubmit(VkQueue,uint32_t n,const VkSubmitInfo* s,VkFence){
 assert(n==1 && s->commandBufferCount==1);calls.push_back("submit");return submitResult;
}
VkResult vkWaitForFences(VkDevice,uint32_t n,const VkFence*,VkBool32 all,uint64_t){
 assert(n==1 && all==VK_TRUE);calls.push_back("wait");return waitResult;
}
VkResult vkInvalidateMappedMemoryRanges(VkDevice,uint32_t n,const VkMappedMemoryRange* r){
 assert(n==1 && r->offset==0 && r->size==VK_WHOLE_SIZE);calls.push_back("invalidate");return invalidateResult;
}
}
int main(){
 std::array<unsigned char,16> staging{},guest{};staging.fill(42);
 AcvReadbackJob job{};job.mappedStaging=staging.data();job.bytes=16;
 auto reset=[&]{calls.clear();guest.fill(99);submitResult=waitResult=invalidateResult=VK_SUCCESS;job.hostCoherent=false;};
 reset();assert(AcvReadbackResolvedComputeBuffer(job,guest.data(),123)==VK_SUCCESS);
 assert(guest==staging && calls==std::vector<std::string>({"gpu-visible","copy","host-visible","end","submit","wait","invalidate"}));
 reset();waitResult=VK_TIMEOUT;
 assert(AcvReadbackResolvedComputeBuffer(job,guest.data(),123)==VK_TIMEOUT);
 for(auto x:guest)assert(x==99);assert(calls.back()=="wait");
 reset();invalidateResult=VK_ERROR_MEMORY_MAP_FAILED;
 assert(AcvReadbackResolvedComputeBuffer(job,guest.data(),123)==VK_ERROR_MEMORY_MAP_FAILED);
 for(auto x:guest)assert(x==99);
 reset();submitResult=VK_ERROR_DEVICE_LOST;
 assert(AcvReadbackResolvedComputeBuffer(job,guest.data(),123)==VK_ERROR_DEVICE_LOST);
 for(auto x:guest)assert(x==99);assert(calls.back()=="submit");
 reset();job.hostCoherent=true;
 assert(AcvReadbackResolvedComputeBuffer(job,guest.data(),123)==VK_SUCCESS);
 assert(guest==staging && calls.back()=="wait");
}
