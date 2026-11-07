#pragma once
// Completion core shared by the SDK and host/real-GPU oracles. Every tracked
// fence MUST come from successful vkQueueSubmit calls on the SAME VkQueue in
// vector order, without reset/reuse until removed here. Other queues require a
// different proof and must not use latest_only.
// Vulkan-Docs090f1b190d60ced4a1d198fd3747d071cc271b1c synchronization.adoc:
// signal-operation-order and synchronization-fences-signaling include earlier
// same-queue fence signal operations in the later fence signal's first scope.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
// SDK callers include this AFTER rex/ui/vulkan/api.h so platform/beta feature
// macros are established before Vulkan headers. Standalone tests use system Vulkan.
#include <vulkan/vulkan_core.h>

namespace acv {
// Shared caller boundary: EndSubmission may report device loss itself. Never
// enter the retirement helper after that loss, even if older fences can still
// report success. The notifier remains owned by EndSubmission in that case.
template<class Current, class End>
bool PrepareSubmissionFenceWait(uint64_t& await_submission, const bool& device_lost,
                                bool submission_open, Current&& current, End&& end) {
  if (device_lost) return false;
  if (await_submission >= current()) {
    if (submission_open) (void)end();
    // An unsuccessful EndSubmission leaves its ticket unsubmitted. Clamping
    // preserves waits on earlier successfully submitted tickets in that case.
    await_submission = current() - 1;
  }
  return !device_lost;
}
struct SubmissionFenceCompletion {
  bool valid_range=true, device_lost=false;
  size_t requested_prefix=0, blocking_fences=0, poll_calls=0, retired=0;
  VkResult blocking_result=VK_SUCCESS, poll_result=VK_SUCCESS;
};
template<class Fence, class Wait, class Poll>
SubmissionFenceCompletion CompleteSubmissionFences(
    std::vector<Fence>& in_flight, std::vector<Fence>& free,
    uint64_t& completed, uint64_t await_submission, bool latest_only, Wait&& wait, Poll&& poll) {
  SubmissionFenceCompletion result;
  const uint64_t required = await_submission > completed ? await_submission-completed : 0;
  if (required > in_flight.size() || required > std::numeric_limits<uint32_t>::max()) {
    result.valid_range=false;return result; // Never index an absent/unsubmitted ticket.
  }
  result.requested_prefix=static_cast<size_t>(required);
  size_t ready=0;
  if (required) {
    result.blocking_fences=latest_only ? 1 : static_cast<size_t>(required);
    const Fence* selected=in_flight.data()+(latest_only ? required-1 : 0);
    result.blocking_result=wait(static_cast<uint32_t>(result.blocking_fences),selected,UINT64_MAX);
    if (result.blocking_result==VK_SUCCESS) ready=static_cast<size_t>(required);
    else if (result.blocking_result==VK_ERROR_DEVICE_LOST) result.device_lost=true;
  }
  // Preserve the existing zero-timeout ordered polling after either a successful
  // blocking wait or a failure. A failed blocking wait itself proves NOTHING;
  // only individually successful polls can advance its earlier prefix.
  while (ready<in_flight.size()) {
    ++result.poll_calls;
    result.poll_result=poll(in_flight[ready]);
    if (result.poll_result!=VK_SUCCESS) {
      if (result.poll_result==VK_ERROR_DEVICE_LOST) result.device_lost=true;
      break;
    }
    ++ready;
  }
  // Device loss retains all fences and does not advance resource completion,
  // including when earlier polls succeeded. Teardown owns those handles.
  if (result.device_lost || !ready) return result;
  free.reserve(free.size()+ready);
  const auto end=in_flight.cbegin()+ready;
  free.insert(free.cend(),in_flight.cbegin(),end);
  in_flight.erase(in_flight.cbegin(),end);
  completed+=ready;result.retired=ready;
  return result;
}
// Legacy callers retain exactly the zero-timeout wait API and native VK_TIMEOUT.
// The optional callback preserves native VK_NOT_READY (no lossy result mapping).
template<class Fence, class Wait>
SubmissionFenceCompletion CompleteSubmissionFences(
    std::vector<Fence>& in_flight, std::vector<Fence>& free,
    uint64_t& completed, uint64_t await_submission, bool latest_only, Wait&& wait) {
  return CompleteSubmissionFences(in_flight, free, completed, await_submission,
      latest_only, wait, [&](Fence fence) { return wait(1, &fence, 0); });
}
} // namespace acv
