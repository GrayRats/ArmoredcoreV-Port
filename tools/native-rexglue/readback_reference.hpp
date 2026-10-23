#pragma once
// Reference only, NOT wired into ACV runtime. Same graphics/compute/transfer queue.
// Caller records the Xenos resolve compute dispatch before invoking this helper.
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstring>

struct AcvReadbackJob {
  VkDevice device;
  VkQueue queue;  // externally synchronized by caller
  VkCommandBuffer command;  // recording, outside a render pass
  VkFence fence;  // unsignaled, not in use
  VkBuffer resolved;  // TRANSFER_SRC, contains resolved guest-format bytes
  VkDeviceSize resolvedOffset;
  VkBuffer staging;  // dedicated TRANSFER_DST buffer, bound at memory offset 0
  VkDeviceMemory stagingMemory;  // HOST_VISIBLE; whole allocation mapped
  const void* mappedStaging;
  VkDeviceSize bytes;
  bool hostCoherent;
};

// All allocations, ranges, source lifetime and generation are validated by caller.
// Staging must be exclusively owned until the fence completes. A timeout/error
// retains all resources and leaves guest memory unpublished for later recovery.
inline VkResult AcvReadbackResolvedComputeBuffer(AcvReadbackJob& j, void* guestOut,
                                                uint64_t timeoutNs) {
  VkBufferMemoryBarrier source{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  source.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  source.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  source.srcQueueFamilyIndex = source.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  source.buffer = j.resolved;
  source.offset = j.resolvedOffset;
  source.size = j.bytes;
  vkCmdPipelineBarrier(j.command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                       0, nullptr, 1, &source, 0, nullptr);
  VkBufferCopy copy{j.resolvedOffset, 0, j.bytes};
  vkCmdCopyBuffer(j.command, j.resolved, j.staging, 1, &copy);

  VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.buffer = j.staging;
  host.offset = 0;
  host.size = j.bytes;
  vkCmdPipelineBarrier(j.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0,
                       0, nullptr, 1, &host, 0, nullptr);
  VkResult result = vkEndCommandBuffer(j.command);
  if (result != VK_SUCCESS) return result;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &j.command;
  result = vkQueueSubmit(j.queue, 1, &submit, j.fence);
  if (result != VK_SUCCESS) return result;
  result = vkWaitForFences(j.device, 1, &j.fence, VK_TRUE, timeoutNs);
  if (result != VK_SUCCESS) return result;
  if (!j.hostCoherent) {
    // Dedicated allocation, bound at 0 and fully mapped: WHOLE_SIZE also
    // handles nonCoherentAtomSize and the allocation's trailing partial atom.
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = j.stagingMemory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    result = vkInvalidateMappedMemoryRanges(j.device, 1, &range);
    if (result != VK_SUCCESS) return result;
  }
  std::memcpy(guestOut, j.mappedStaging, static_cast<size_t>(j.bytes));
  // Caller now publishes CPU generation under its range lock and notifies
  // existing guest-memory watches; never publish before this copy completes.
  return VK_SUCCESS;
}
