#pragma once
#include "timeline_submission.h"
#include <algorithm>
#include <array>
#include <limits>
namespace acv {
// Queue 1 of the graphics family. Shared buffers retain exclusive ownership.
// All calls run on the GPU worker; host publication uses only proven
// completion.
class AsyncReadbackTransfer {
  struct Record {
    VkCommandPool pool{};
    VkCommandBuffer command{};
    uint64_t value = 0, ticket = 0;
  };
  VkDevice device_;
  TimelineSubmission timeline_;
  std::array<Record, 64> records_{};
  PFN_vkCreateCommandPool create_{};
  PFN_vkDestroyCommandPool destroy_{};
  PFN_vkAllocateCommandBuffers allocate_{};
  PFN_vkResetCommandPool reset_{};
  PFN_vkBeginCommandBuffer begin_{};
  PFN_vkEndCommandBuffer end_{};
  PFN_vkQueueWaitIdle idle_{};
  PFN_vkCmdCopyBuffer copy_{};
  PFN_vkCmdPipelineBarrier barrier_{};
  uint32_t family_;
  uint64_t completed_ = 0, completed_ticket_ = 0;
  bool used_ = false;
  void Cleanup() {
    for (auto &r : records_)
      if (r.pool) {
        destroy_(device_, r.pool, nullptr);
        r = {};
      }
    timeline_.DestroyAfterDeviceStopped();
  }

public:
  VkResult result = VK_SUCCESS;
  AsyncReadbackTransfer(VkDevice device, PFN_vkGetDeviceProcAddr proc,
                        uint32_t family, uint64_t max_difference)
      : device_(device), timeline_(device, proc, 0, max_difference),
        family_(family) {
#define ACV_TRANSFER_LOAD(field, name)                                         \
  field = reinterpret_cast<PFN_##name>(proc(device, #name))
    ACV_TRANSFER_LOAD(create_, vkCreateCommandPool);
    ACV_TRANSFER_LOAD(destroy_, vkDestroyCommandPool);
    ACV_TRANSFER_LOAD(allocate_, vkAllocateCommandBuffers);
    ACV_TRANSFER_LOAD(reset_, vkResetCommandPool);
    ACV_TRANSFER_LOAD(begin_, vkBeginCommandBuffer);
    ACV_TRANSFER_LOAD(end_, vkEndCommandBuffer);
    ACV_TRANSFER_LOAD(idle_, vkQueueWaitIdle);
    ACV_TRANSFER_LOAD(copy_, vkCmdCopyBuffer);
    ACV_TRANSFER_LOAD(barrier_, vkCmdPipelineBarrier);
#undef ACV_TRANSFER_LOAD
    result = timeline_.initialization;
    if (result == VK_SUCCESS &&
        (!create_ || !destroy_ || !allocate_ || !reset_ || !begin_ || !end_ ||
         !idle_ || !copy_ || !barrier_))
      result = VK_ERROR_INITIALIZATION_FAILED;
  }
  ~AsyncReadbackTransfer() {
    if (!used_)
      Cleanup();
  }
  VkSemaphore handle() const { return timeline_.handle(); }
  uint64_t latest() const { return timeline_.last_submitted(); }
  bool ready() const { return result == VK_SUCCESS && handle(); }
  bool PublishedReady(uint64_t ticket) const {
    for (const auto &r : records_)
      if (r.ticket == ticket)
        return r.value <= completed_;
    return ticket <= completed_ticket_;
  }
  VkResult WaitForTicket(uint64_t ticket) {
    uint64_t value = 0;
    for (const auto &r : records_)
      if (r.ticket <= ticket)
        value = std::max(value, r.value);
    if (value > completed_) {
      result = timeline_.Wait(value, UINT64_MAX);
      if (result != VK_SUCCESS)
        return result;
    }
    result = timeline_.Counter(completed_);
    if (result != VK_SUCCESS)
      return result;
    for (const auto &r : records_)
      if (r.value <= completed_)
        completed_ticket_ = std::max(completed_ticket_, r.ticket);
    return VK_SUCCESS;
  }
  VkResult Submit(VkQueue queue, VkSemaphore producer, uint64_t ticket,
                  VkBuffer source, VkBuffer target,
                  const VkBufferCopy &region) {
    if (failed_)
      return failure_;
    VkResult status =
        SubmitImpl(queue, producer, ticket, source, target, region);
    if (status != VK_SUCCESS) {
      failed_ = true;
      failure_ = status;
    }
    return status;
  }

private:
  bool failed_ = false;
  VkResult failure_ = VK_SUCCESS;
  VkResult SubmitImpl(VkQueue queue, VkSemaphore producer, uint64_t ticket,
                      VkBuffer source, VkBuffer target,
                      const VkBufferCopy &region) {
    result = timeline_.Counter(completed_);
    if (result != VK_SUCCESS)
      return result;
    for (const auto &r : records_)
      if (r.value <= completed_)
        completed_ticket_ = std::max(completed_ticket_, r.ticket);
    if (latest() == UINT64_MAX)
      return result = VK_ERROR_TOO_MANY_OBJECTS;
    uint64_t next = latest() + 1;
    if (next - completed_ > timeline_.maximum_difference()) {
      result =
          timeline_.Wait(next - timeline_.maximum_difference(), UINT64_MAX);
      if (result != VK_SUCCESS)
        return result;
      result = timeline_.Counter(completed_);
      if (result != VK_SUCCESS)
        return result;
    }
    Record *record = nullptr;
    for (auto &r : records_)
      if (r.value <= completed_) {
        record = &r;
        break;
      }
    if (!record) {
      auto *oldest = &records_[0];
      for (auto &r : records_)
        if (r.value < oldest->value)
          oldest = &r;
      result = WaitForTicket(oldest->ticket);
      if (result != VK_SUCCESS)
        return result;
      record = oldest;
    }
    if (!record->pool) {
      VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      pi.queueFamilyIndex = family_;
      pi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      result = create_(device_, &pi, nullptr, &record->pool);
      if (result != VK_SUCCESS) {
        record->pool = VK_NULL_HANDLE;
        return result;
      }
      VkCommandBufferAllocateInfo ai{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      ai.commandPool = record->pool;
      ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      ai.commandBufferCount = 1;
      result = allocate_(device_, &ai, &record->command);
      if (result != VK_SUCCESS) {
        destroy_(device_, record->pool, nullptr);
        *record = {};
        return result;
      }
    } else {
      result = reset_(device_, record->pool, 0);
      if (result != VK_SUCCESS)
        return result;
    }
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = begin_(record->command, &bi);
    if (result != VK_SUCCESS)
      return result;
    copy_(record->command, source, target, 1, &region);
    VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex = host.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    host.buffer = target;
    host.offset = region.dstOffset;
    host.size = region.size;
    barrier_(record->command, VK_PIPELINE_STAGE_TRANSFER_BIT,
             VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host, 0, nullptr);
    result = end_(record->command);
    if (result != VK_SUCCESS)
      return result;
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &producer;
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &record->command;
    // Retain resources on unknown/device-loss results until explicit device
    // stop.
    used_ = true;
    result = timeline_.Submit(queue, si, next, completed_, &ticket);
    if (result == VK_SUCCESS) {
      record->value = next;
      record->ticket = ticket;
    }
    return result;
  }

public:
  VkResult Stop(VkQueue queue) { return idle_(queue); }
  void DestroyAfterDeviceStopped() {
    Cleanup();
    used_ = false;
  }
};
} // namespace acv
