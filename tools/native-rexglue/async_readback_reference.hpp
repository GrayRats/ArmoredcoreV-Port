#pragma once
// Reference only; not wired into the SDK or game. Vulkan 1.0 entry points.
// One externally synchronized graphics/compute/transfer queue. Caller owns all
// handles, command pools, source state tracking and the ring of slots.
#include <vulkan/vulkan.h>
#include <cstdint>
#include <type_traits>

struct AcvResourceVersion {
  uint64_t resourceId, allocationEpoch, gpuVersion, cpuWriteEpoch;
  bool operator==(const AcvResourceVersion& b) const {
    return resourceId==b.resourceId && allocationEpoch==b.allocationEpoch &&
           gpuVersion==b.gpuVersion && cpuWriteEpoch==b.cpuWriteEpoch;
  }
};
struct AcvAsyncReadbackSlot {
  VkBuffer staging; // TRANSFER_DST, bound at offset 0 to a dedicated allocation
  VkDeviceMemory memory; // HOST_VISIBLE, entire allocation persistently mapped
  VkFence fence; // exclusive to this slot, never reset while busy
  const void* mapped;
  VkDeviceSize capacity, bytes;
  AcvResourceVersion version;
  bool coherent, busy=false, poisoned=false;
};
inline void AcvHostReadBarrier(VkCommandBuffer cmd,VkBuffer buffer,VkDeviceSize bytes) {
  VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
  b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  b.buffer=buffer;b.offset=0;b.size=bytes;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                       0,0,nullptr,1,&b,0,nullptr);
}
// Actual FSI integration uses a resolved guest-format VkBuffer, not a VkImage.
// Caller validates source range, holds its state lock, and restores/updates the
// SDK usage tracker before subsequent accesses. This is a recording helper.
inline void AcvRecordResolvedRange(VkCommandBuffer cmd,VkBuffer resolved,
                                   VkDeviceSize offset,VkDeviceSize bytes,
                                   VkBuffer staging) {
  VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  b.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  b.buffer=resolved;b.offset=offset;b.size=bytes;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&b,0,nullptr);
  VkBufferCopy copy{offset,0,bytes};vkCmdCopyBuffer(cmd,resolved,staging,1,&copy);
  AcvHostReadBarrier(cmd,staging,bytes);
}
// Alternative ordinary-image path: single-sample RGBA8 color image, mip0/layer0,
// TRANSFER_SRC usage, COLOR_ATTACHMENT_OPTIMAL on entry and exit. Caller validates
// rectangle bounds and staging capacity; color attachment contents must be stored.
// End rendering before calling. Compressed/MSAA/depth/multiplanar need other paths.
inline void AcvRecordColorRect(VkCommandBuffer cmd,VkImage image,VkBuffer staging,
                              VkOffset3D origin,VkExtent3D extent) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
  VkBufferImageCopy copy{};
  copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
  copy.imageOffset=origin;copy.imageExtent=extent;
  // bufferRowLength/ImageHeight=0 pack only this rectangle into staging.
  vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging,1,&copy);
  b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       0,0,nullptr,0,nullptr,1,&b);
  AcvHostReadBarrier(cmd,staging,VkDeviceSize(extent.width)*extent.height*4);
}
// command is executable. Submit after the producer on the SAME queue, so no
// extra semaphore is needed. Queue submission is externally synchronized.
inline VkResult AcvSubmitReadback(VkDevice device,VkQueue queue,VkCommandBuffer command,
                                  AcvAsyncReadbackSlot& slot,VkDeviceSize bytes,
                                  AcvResourceVersion version) {
  if(slot.busy || slot.poisoned)return VK_NOT_READY;
  if(!bytes || bytes>slot.capacity)return VK_ERROR_INITIALIZATION_FAILED;
  VkResult r=vkResetFences(device,1,&slot.fence);
  if(r!=VK_SUCCESS){slot.poisoned=true;return r;}
  VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  s.commandBufferCount=1;s.pCommandBuffers=&command;
  r=vkQueueSubmit(queue,1,&s,slot.fence);
  if(r!=VK_SUCCESS){slot.poisoned=true;return r;}
  slot.bytes=bytes;slot.version=version;slot.busy=true;return VK_SUCCESS;
}
enum class AcvPollState { Idle, Pending, Published, Stale, Failed };
struct AcvPollResult { AcvPollState state;VkResult status; };
// Hold the source allocation/version lock across checking current and consume.
// Callback is noexcept and scatters the packed rect (or guest-format resolved
// range) to the correct guest destination; updates only this CPU generation/range.
// Stale work never overwrites a reused allocation or a newer CPU write.
template<class Consume>
AcvPollResult AcvTryConsumeReadback(VkDevice device,AcvAsyncReadbackSlot& slot,
                                   AcvResourceVersion current,Consume consume) {
  static_assert(std::is_nothrow_invocable_v<Consume,const void*,VkDeviceSize>);
  if(slot.poisoned)return {AcvPollState::Failed,VK_ERROR_INITIALIZATION_FAILED};
  if(!slot.busy)return {AcvPollState::Idle,VK_SUCCESS};
  VkResult r=vkGetFenceStatus(device,slot.fence);
  if(r==VK_NOT_READY)return {AcvPollState::Pending,r};
  if(r!=VK_SUCCESS){slot.poisoned=true;return {AcvPollState::Failed,r};}
  if(!(slot.version==current)) {
    slot.busy=false;return {AcvPollState::Stale,VK_SUCCESS};
  }
  if(!slot.coherent) {
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory=slot.memory;range.offset=0;range.size=VK_WHOLE_SIZE;
    r=vkInvalidateMappedMemoryRanges(device,1,&range);
    if(r!=VK_SUCCESS){slot.poisoned=true;return {AcvPollState::Failed,r};}
  }
  consume(slot.mapped,slot.bytes);
  slot.busy=false;return {AcvPollState::Published,VK_SUCCESS};
}
// Never recycle command buffers, staging or source bindings while their jobs are
// pending. Destroy resources only after completion (or device-loss recovery).
// If all slots are busy, defer/merge work; do not overwrite an in-flight slot.
