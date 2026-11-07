#include "async_readback_transfer.h"
#include "timeline_submission.h"
#include "vertex_input_pipeline_libraries.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
using namespace acv;
namespace {
uintptr_t next_handle = 100;
std::map<VkSemaphore, uint64_t> counters;
std::map<VkSemaphore, uint64_t> submitted;
VkResult submit_result = VK_SUCCESS;
bool complete = false;
uint64_t submits = 0, destroyed = 0, waits = 0, pools = 0, library_creates = 0,
         final_creates = 0;
bool fail_library = false;
template <class T> T Handle() { return reinterpret_cast<T>(++next_handle); }
VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice, const VkSemaphoreCreateInfo *ci,
                                      const VkAllocationCallbacks *,
                                      VkSemaphore *sem) {
  auto *type = (const VkSemaphoreTypeCreateInfo *)ci->pNext;
  assert(type && type->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE);
  *sem = Handle<VkSemaphore>();
  counters[*sem] = submitted[*sem] = type->initialValue;
  return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice, VkSemaphore sem,
                                   const VkAllocationCallbacks *) {
  ++destroyed;
  counters.erase(sem);
  submitted.erase(sem);
}
VKAPI_ATTR VkResult VKAPI_CALL Counter(VkDevice, VkSemaphore sem,
                                       uint64_t *value) {
  *value = counters.at(sem);
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL Wait(VkDevice, const VkSemaphoreWaitInfo *info,
                                    uint64_t) {
  ++waits;
  for (unsigned i = 0; i < info->semaphoreCount; ++i) {
    assert(info->pValues[i] <= submitted.at(info->pSemaphores[i]));
    counters[info->pSemaphores[i]] = info->pValues[i];
  }
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL Submit(VkQueue, uint32_t count,
                                      const VkSubmitInfo *info, VkFence f) {
  ++submits;
  assert(count == 1 && !f);
  auto *values = (const VkTimelineSemaphoreSubmitInfo *)info->pNext;
  assert(values &&
         values->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO &&
         values->waitSemaphoreValueCount == info->waitSemaphoreCount);
  if (submit_result != VK_SUCCESS)
    return submit_result;
  for (unsigned i = 0; i < info->signalSemaphoreCount; ++i) {
    submitted[info->pSignalSemaphores[i]] = values->pSignalSemaphoreValues[i];
    if (complete)
      counters[info->pSignalSemaphores[i]] = values->pSignalSemaphoreValues[i];
  }
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL CreatePool(VkDevice,
                                          const VkCommandPoolCreateInfo *,
                                          const VkAllocationCallbacks *,
                                          VkCommandPool *p) {
  ++pools;
  *p = Handle<VkCommandPool>();
  return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL DestroyPool(VkDevice, VkCommandPool,
                                       const VkAllocationCallbacks *) {
  --pools;
}
VKAPI_ATTR VkResult VKAPI_CALL Allocate(VkDevice,
                                        const VkCommandBufferAllocateInfo *,
                                        VkCommandBuffer *c) {
  *c = Handle<VkCommandBuffer>();
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL Reset(VkDevice, VkCommandPool,
                                     VkCommandPoolResetFlags) {
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL Begin(VkCommandBuffer,
                                     const VkCommandBufferBeginInfo *) {
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL End(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL Copy(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t,
                                const VkBufferCopy *) {}
VKAPI_ATTR void VKAPI_CALL Barrier(VkCommandBuffer, VkPipelineStageFlags,
                                   VkPipelineStageFlags, VkDependencyFlags,
                                   uint32_t, const VkMemoryBarrier *, uint32_t,
                                   const VkBufferMemoryBarrier *, uint32_t,
                                   const VkImageMemoryBarrier *) {}
VKAPI_ATTR VkResult VKAPI_CALL Idle(VkQueue) { return VK_SUCCESS; }
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Proc(VkDevice, const char *name) {
#define F(n, f)                                                                \
  if (!std::strcmp(name, n))                                                   \
  return reinterpret_cast<PFN_vkVoidFunction>(f)
  F("vkCreateSemaphore", Create);
  F("vkDestroySemaphore", Destroy);
  F("vkGetSemaphoreCounterValue", Counter);
  F("vkWaitSemaphores", Wait);
  F("vkQueueSubmit", Submit);
  F("vkCreateCommandPool", CreatePool);
  F("vkDestroyCommandPool", DestroyPool);
  F("vkAllocateCommandBuffers", Allocate);
  F("vkResetCommandPool", Reset);
  F("vkBeginCommandBuffer", Begin);
  F("vkEndCommandBuffer", End);
  F("vkCmdCopyBuffer", Copy);
  F("vkCmdPipelineBarrier", Barrier);
  F("vkQueueWaitIdle", Idle);
#undef F
  return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Missing(VkDevice, const char *) {
  return nullptr;
}
VKAPI_ATTR VkResult VKAPI_CALL Pipeline(VkDevice, VkPipelineCache, uint32_t n,
                                        const VkGraphicsPipelineCreateInfo *i,
                                        const VkAllocationCallbacks *,
                                        VkPipeline *p) {
  assert(n == 1);
  if (i->flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) {
    ++library_creates;
    if (fail_library) {
      *p = VK_NULL_HANDLE;
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
  } else {
    ++final_creates;
    assert(!i->pVertexInputState && !i->pInputAssemblyState);
  }
  *p = Handle<VkPipeline>();
  return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline,
                                           const VkAllocationCallbacks *) {}
} // namespace
int main() {
  VkDevice device = Handle<VkDevice>();
  VkQueue queue = Handle<VkQueue>();
  {
    TimelineSubmission missing(device, Missing, 0, 16);
    assert(missing.initialization != VK_SUCCESS && !missing.handle());
  }
  {
    TimelineSubmission t(device, Proc, 7, 4);
    assert(t.initialization == VK_SUCCESS);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    assert(t.Wait(8, 0) == VK_ERROR_INITIALIZATION_FAILED);
    assert(t.Submit(queue, si, 7, 7) == VK_ERROR_INITIALIZATION_FAILED);
    assert(t.Submit(queue, si, 12, 7) == VK_ERROR_INITIALIZATION_FAILED);
    submit_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    assert(t.Submit(queue, si, 8, 7) == submit_result);
    assert(t.last_submitted() == 7);
    submit_result = VK_SUCCESS;
    assert(t.Submit(queue, si, 8, 7) == VK_SUCCESS);
    uint64_t value = 99;
    assert(t.Counter(value) == VK_SUCCESS && value == 7);
    assert(t.Wait(8, 100) == VK_SUCCESS);
    assert(t.Counter(value) == VK_SUCCESS && value == 8);
    t.DestroyAfterDeviceStopped();
  }
  {
    AsyncReadbackTransfer transfer(device, Proc, 0, 64);
    assert(transfer.ready());
    auto producer = Handle<VkSemaphore>();
    VkBufferCopy region{0, 256, 1024};
    for (uint64_t i = 1; i <= 64; ++i)
      assert(transfer.Submit(queue, producer, i, Handle<VkBuffer>(),
                             Handle<VkBuffer>(), region) == VK_SUCCESS);
    assert(pools == 64 && !transfer.PublishedReady(64));
    assert(transfer.WaitForTicket(32) == VK_SUCCESS &&
           transfer.PublishedReady(32) && !transfer.PublishedReady(64));
    assert(transfer.Submit(queue, producer, 65, Handle<VkBuffer>(),
                           Handle<VkBuffer>(), region) == VK_SUCCESS);
    assert(pools == 64);
    submit_result = VK_ERROR_DEVICE_LOST;
    assert(transfer.Submit(queue, producer, 66, Handle<VkBuffer>(),
                           Handle<VkBuffer>(), region) == VK_ERROR_DEVICE_LOST);
    assert(transfer.latest() == 65);
    uint64_t before = submits;
    assert(transfer.Submit(queue, producer, 67, Handle<VkBuffer>(),
                           Handle<VkBuffer>(), region) == VK_ERROR_DEVICE_LOST);
    assert(submits == before);

    submit_result = VK_SUCCESS;
    transfer.DestroyAfterDeviceStopped();
    assert(pools == 0);
  }
  {
    VertexInputPipelineLibraries libraries(device, Pipeline, DestroyPipeline);
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkGraphicsPipelineCreateInfo pi{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    VkPipeline p{};
    VkResult r;
    assert(libraries.TryCreate(pi, VK_NULL_HANDLE, p, r) && r == VK_SUCCESS);
    assert(libraries.TryCreate(pi, VK_NULL_HANDLE, p, r) && r == VK_SUCCESS);
    assert(library_creates == 1 && final_creates == 2);
    ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    assert(libraries.TryCreate(pi, VK_NULL_HANDLE, p, r) && r == VK_SUCCESS);
    assert(library_creates == 2);
    ia.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    fail_library = true;
    assert(libraries.TryCreate(pi, VK_NULL_HANDLE, p, r) &&
           r == VK_ERROR_OUT_OF_DEVICE_MEMORY);
    fail_library = false;
    assert(libraries.TryCreate(pi, VK_NULL_HANDLE, p, r) && r == VK_SUCCESS);
    VkDynamicState state = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
    VkPipelineDynamicStateCreateInfo dy{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 1;
    dy.pDynamicStates = &state;
    pi.pDynamicState = &dy;
    assert(!libraries.TryCreate(pi, VK_NULL_HANDLE, p, r));
  }
  assert(counters.empty());
  std::cout << "PASS missing PFN, monotonic/range admission, failed submit "
               "retention, no premature publication, 64 pending transfer "
               "slots/reuse/device loss, GPL key/reuse/failure/fallback\n";
}
