// Mock Vulkan contract tests. No real GPU/driver or game performance claim.
#include "async_readback_reference.hpp"
#include <array>
#include <cassert>
#include <cstring>
static VkResult fenceStatus=VK_NOT_READY,submitStatus=VK_SUCCESS,
                resetStatus=VK_SUCCESS,invalidateStatus=VK_SUCCESS;
static unsigned invalidates=0,resets=0,submits=0,hostBarriers=0,imageCopies=0,bufferCopies=0;
extern "C" {
void vkCmdPipelineBarrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags dst,
 VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t n,const VkBufferMemoryBarrier* b,
 uint32_t m,const VkImageMemoryBarrier* i) {
 if(n && dst==VK_PIPELINE_STAGE_HOST_BIT) {
  assert(b->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT && b->dstAccessMask==VK_ACCESS_HOST_READ_BIT);
  ++hostBarriers;
 }
 if(m)assert(i->oldLayout!=VK_IMAGE_LAYOUT_UNDEFINED);
}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer,VkBuffer,uint32_t n,const VkBufferCopy* r) {
 assert(n==1 && r->srcOffset==32 && r->dstOffset==0 && r->size==16);++bufferCopies;
}
void vkCmdCopyImageToBuffer(VkCommandBuffer,VkImage,VkImageLayout layout,VkBuffer,
 uint32_t n,const VkBufferImageCopy* r) {
 assert(layout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && n==1);
 assert(r->imageOffset.x==8 && r->imageExtent.width==16 && r->imageExtent.height==8);
 assert(r->bufferRowLength==0 && r->bufferImageHeight==0);++imageCopies;
}
VkResult vkResetFences(VkDevice,uint32_t n,const VkFence*){assert(n==1);++resets;return resetStatus;}
VkResult vkQueueSubmit(VkQueue,uint32_t n,const VkSubmitInfo* s,VkFence){assert(n==1 && s->commandBufferCount==1);++submits;return submitStatus;}
VkResult vkGetFenceStatus(VkDevice,VkFence){return fenceStatus;}
VkResult vkInvalidateMappedMemoryRanges(VkDevice,uint32_t n,const VkMappedMemoryRange* r){
 assert(n==1 && r->offset==0 && r->size==VK_WHOLE_SIZE);++invalidates;return invalidateStatus;
}
}
int main(){
 std::array<unsigned char,16> staging{},guest{};staging.fill(42);guest.fill(99);
 AcvResourceVersion version{7,2,10,5};
 AcvAsyncReadbackSlot slot{};slot.mapped=staging.data();slot.capacity=16;
 auto consume=[&](const void* data,VkDeviceSize n) noexcept {assert(n==16);std::memcpy(guest.data(),data,16);};
 auto submit=[&]{return AcvSubmitReadback(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,slot,16,version);};
 assert(submit()==VK_SUCCESS && slot.busy);
 assert(submit()==VK_NOT_READY && submits==1 && resets==1); // no ring overwrite/reset
 auto p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,version,consume);
 assert(p.state==AcvPollState::Pending && guest[0]==99 && invalidates==0);
 fenceStatus=VK_SUCCESS;p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,version,consume);
 assert(p.state==AcvPollState::Published && guest==staging && !slot.busy && invalidates==1);
 for(unsigned field=0;field<3;++field) {
  guest.fill(99);assert(submit()==VK_SUCCESS);auto changed=version;
  if(field==0)++changed.allocationEpoch;
  if(field==1)++changed.gpuVersion;
  if(field==2)++changed.cpuWriteEpoch;
  p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,changed,consume);
  assert(p.state==AcvPollState::Stale && guest[0]==99 && !slot.busy && invalidates==1);
 }
 slot.coherent=true;assert(submit()==VK_SUCCESS);fenceStatus=VK_NOT_READY;
 p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,version,consume);
 assert(p.state==AcvPollState::Pending && guest[0]==99);
 fenceStatus=VK_SUCCESS;p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,version,consume);
 assert(p.state==AcvPollState::Published && invalidates==1);
 slot.coherent=false;guest.fill(99);assert(submit()==VK_SUCCESS);
 invalidateStatus=VK_ERROR_MEMORY_MAP_FAILED;
 p=AcvTryConsumeReadback(VK_NULL_HANDLE,slot,version,consume);
 assert(p.state==AcvPollState::Failed && slot.poisoned && guest[0]==99);
 assert(submit()==VK_NOT_READY);
 slot={};slot.capacity=16;submitStatus=VK_ERROR_DEVICE_LOST;
 assert(submit()==VK_ERROR_DEVICE_LOST && slot.poisoned);
 slot={};slot.capacity=16;submitStatus=VK_SUCCESS;resetStatus=VK_ERROR_DEVICE_LOST;
 assert(submit()==VK_ERROR_DEVICE_LOST && slot.poisoned);
 AcvRecordColorRect(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,{8,0,0},{16,8,1});
 AcvRecordResolvedRange(VK_NULL_HANDLE,VK_NULL_HANDLE,32,16,VK_NULL_HANDLE);
 assert(imageCopies==1 && bufferCopies==1 && hostBarriers==2);
}
