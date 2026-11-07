#pragma once
// Device feature must have been enabled before constructing this owner.
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace acv {
class TimelineSubmission {
  VkDevice device_;
  VkSemaphore semaphore_{};
  PFN_vkCreateSemaphore create_{};
  PFN_vkDestroySemaphore destroy_{};
  PFN_vkGetSemaphoreCounterValue counter_{};
  PFN_vkWaitSemaphores wait_{};
  PFN_vkQueueSubmit submit_{};
  uint64_t initial_, submitted_, max_difference_;

public:
  VkResult initialization = VK_SUCCESS;
  TimelineSubmission(VkDevice device, PFN_vkGetDeviceProcAddr proc,
                     uint64_t initial, uint64_t max_difference)
      : device_(device), initial_(initial), submitted_(initial),
        max_difference_(max_difference) {
    create_ = reinterpret_cast<PFN_vkCreateSemaphore>(
        proc(device, "vkCreateSemaphore"));
    destroy_ = reinterpret_cast<PFN_vkDestroySemaphore>(
        proc(device, "vkDestroySemaphore"));
    counter_ = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
        proc(device, "vkGetSemaphoreCounterValue"));
    wait_ = reinterpret_cast<PFN_vkWaitSemaphores>(
        proc(device, "vkWaitSemaphores"));
    submit_ =
        reinterpret_cast<PFN_vkQueueSubmit>(proc(device, "vkQueueSubmit"));
    if (!create_ || !destroy_ || !counter_ || !wait_ || !submit_ ||
        !max_difference_) {
      initialization = VK_ERROR_INITIALIZATION_FAILED;
      return;
    }
    VkSemaphoreTypeCreateInfo type{
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type.initialValue = initial;
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    info.pNext = &type;
    initialization = create_(device_, &info, nullptr, &semaphore_);
    if (initialization != VK_SUCCESS)
      semaphore_ = VK_NULL_HANDLE;
  }
  // Normal teardown explicitly proves stop. Unexpected destruction retains a
  // potentially GPU-referenced handle instead of freeing it while pending.
  ~TimelineSubmission() {
    if (semaphore_ && submitted_ == initial_)
      destroy_(device_, semaphore_, nullptr);
  }
  TimelineSubmission(const TimelineSubmission &) = delete;
  TimelineSubmission &operator=(const TimelineSubmission &) = delete;
  VkSemaphore handle() const { return semaphore_; }
  uint64_t maximum_difference() const { return max_difference_; }
  uint64_t last_submitted() const { return submitted_; }
  void DestroyAfterDeviceStopped() {
    if (semaphore_)
      destroy_(device_, semaphore_, nullptr);
    semaphore_ = VK_NULL_HANDLE;
  }
  VkResult Counter(uint64_t &value) const {
    return counter_(device_, semaphore_, &value);
  }
  VkResult Wait(uint64_t ticket, uint64_t timeout) const {
    if (ticket > submitted_)
      return VK_ERROR_INITIALIZATION_FAILED;
    VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    info.semaphoreCount = 1;
    info.pSemaphores = &semaphore_;
    info.pValues = &ticket;
    return wait_(device_, &info, timeout);
  }
  // Existing CP submit has only binary waits and no signals. Their zero
  // timeline values are ignored by Vulkan, but the count must match all wait
  // semaphores.
  VkResult Submit(VkQueue queue, const VkSubmitInfo &original, uint64_t ticket,
                  uint64_t known_completed,
                  const uint64_t *wait_values = nullptr) {
    if (!semaphore_ || ticket <= submitted_ || ticket < known_completed ||
        ticket - known_completed > max_difference_ || original.pNext ||
        original.signalSemaphoreCount)
      return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<uint64_t> waits;
    try {
      waits.resize(original.waitSemaphoreCount, 0);
    } catch (...) {
      return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    VkTimelineSemaphoreSubmitInfo values{
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    values.waitSemaphoreValueCount = original.waitSemaphoreCount;
    values.pWaitSemaphoreValues =
        wait_values ? wait_values : (waits.empty() ? nullptr : waits.data());
    values.signalSemaphoreValueCount = 1;
    values.pSignalSemaphoreValues = &ticket;
    VkSubmitInfo info = original;
    info.pNext = &values;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &semaphore_;
    VkResult result = submit_(queue, 1, &info, VK_NULL_HANDLE);
    if (result == VK_SUCCESS)
      submitted_ = ticket;
    return result;
  }
};
} // namespace acv
