#pragma once
// Standalone Vulkan readback test fixture. Vulkan 1.1, first discrete GPU,
// graphics queue, no optional extensions/features or validation-layer assumption.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace acv::readback_test {
using Clock = std::chrono::steady_clock;
void Require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
void Check(VkResult result, const char* what) {
  if (result != VK_SUCCESS) throw std::runtime_error(std::string(what)+": VkResult="+std::to_string(result));
}
#define VK_CHECK(expr) Check((expr), #expr)
struct Buffer { VkBuffer handle{}; VkDeviceMemory memory{}; void* mapping{}; uint32_t type{}; };
struct Vulkan {
  VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{};
  VkQueue queue{}; VkCommandPool pool{}; VkCommandBuffer command{}; VkFence fence{};
  VkPhysicalDeviceProperties properties{}; VkPhysicalDeviceMemoryProperties memory_properties{};
  std::array<Buffer, 2> buffers{};
  bool pending = false;
  void Init() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
    VK_CHECK(vkCreateInstance(&ici,nullptr,&instance));
    uint32_t count=0; VK_CHECK(vkEnumeratePhysicalDevices(instance,&count,nullptr));
    Require(count!=0,"no Vulkan devices"); std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance,&count,devices.data()));
    for (auto candidate:devices) {
      VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(candidate,&props);
      if (props.apiVersion>=VK_API_VERSION_1_1 && props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physical=candidate; properties=props; break;
      }
    }
    Require(physical!=VK_NULL_HANDLE,"benchmark requires a discrete Vulkan 1.1 GPU");
    vkGetPhysicalDeviceMemoryProperties(physical,&memory_properties);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
    uint32_t family=0;
    while (family<count && !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) ++family;
    Require(family<count,"no graphics queue"); float priority=1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex=family; qci.queueCount=1; qci.pQueuePriorities=&priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
    VK_CHECK(vkCreateDevice(physical,&dci,nullptr,&device)); vkGetDeviceQueue(device,family,0,&queue);
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex=family; pci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(device,&pci,nullptr,&pool));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool=pool; cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount=1;
    VK_CHECK(vkAllocateCommandBuffers(device,&cai,&command));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VK_CHECK(vkCreateFence(device,&fci,nullptr,&fence));
    std::cout<<"{\"device\":\""<<properties.deviceName<<"\",\"api_version\":"<<properties.apiVersion
      <<",\"driver_version\":"<<properties.driverVersion<<",\"non_coherent_atom_size\":"
      <<properties.limits.nonCoherentAtomSize<<",\"validation_requested\":false}\n";
  }
  void Allocate(size_t index, size_t size, bool host) {
    auto& b=buffers.at(index);
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; ci.size=size;
    ci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|(host?0:VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device,&ci,nullptr,&b.handle));
    VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device,b.handle,&requirements);
    // Match SDK util.h ChooseHostMemoryType(kReadback): first HOST_VISIBLE |
    // HOST_CACHED supported type, falling back to first HOST_VISIBLE type.
    uint32_t type=UINT32_MAX;
    for (unsigned pass=0;pass<2 && type==UINT32_MAX;++pass) {
      auto flags=host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|(pass?0:VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                      : (pass?0:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      for (uint32_t i=0;i<memory_properties.memoryTypeCount;++i) {
        if ((requirements.memoryTypeBits&(1u<<i)) && (memory_properties.memoryTypes[i].propertyFlags&flags)==flags) {
          type=i; break;
        }
      }
    }
    Require(type!=UINT32_MAX,"no suitable memory type"); b.type=type;
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.buffer=b.handle;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.pNext=&dedicated; ai.allocationSize=requirements.size; ai.memoryTypeIndex=type;
    VK_CHECK(vkAllocateMemory(device,&ai,nullptr,&b.memory));
    VK_CHECK(vkBindBufferMemory(device,b.handle,b.memory,0));
    if(host) VK_CHECK(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapping));
    std::cout<<"{\"buffer\":"<<index<<",\"memory_type\":"<<type<<",\"memory_flags\":"
      <<memory_properties.memoryTypes[type].propertyFlags<<",\"allocation\":"<<requirements.size
      <<",\"mapping_mod_4096\":"<<(reinterpret_cast<uintptr_t>(b.mapping)%4096)<<"}\n";
    if(host) Require(reinterpret_cast<uintptr_t>(b.mapping)%4096==0,"staging mapping is not observed 4KiB alignment");
  }
  void Begin() {
    Require(!pending,"command buffer still pending"); VK_CHECK(vkResetCommandBuffer(command,0));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(command,&begin));
  }
  void Barrier(VkBuffer buffer, VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags target) {
    VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    b.srcAccessMask=src; b.dstAccessMask=dst; b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.buffer=buffer; b.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,target,0,0,nullptr,1,&b,0,nullptr);
  }
  void SubmitWait() {
    VK_CHECK(vkEndCommandBuffer(command)); VK_CHECK(vkResetFences(device,1,&fence));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
    VK_CHECK(vkQueueSubmit(queue,1,&submit,fence)); pending=true;
    VK_CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,5'000'000'000ULL)); pending=false;
  }
  void Stage(size_t offset, size_t bytes) {
    Begin(); VkBufferCopy region{0,offset,bytes};
    vkCmdCopyBuffer(command,buffers[0].handle,buffers[1].handle,1,&region);
    Barrier(buffers[1].handle,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT);
    SubmitWait();
    // Same unconditional invalidate as GeneralReadback; offset=0 + WHOLE_SIZE
    // covers atom alignment and the entire mapped allocation even if coherent.
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=buffers[1].memory; range.size=VK_WHOLE_SIZE;
    VK_CHECK(vkInvalidateMappedMemoryRanges(device,1,&range));
  }
  ~Vulkan() {
    // A timed-out submitted job cannot legally be freed while pending. Fail the
    // standalone process without touching pending handles; OS/driver own cleanup.
    if (pending) { std::cerr<<"pending GPU submission at teardown; terminating benchmark\n"; std::_Exit(2); }
    if(device) {
      for(auto& b:buffers) {
        if(b.mapping) vkUnmapMemory(device,b.memory);
        if(b.handle) vkDestroyBuffer(device,b.handle,nullptr);
        if(b.memory) vkFreeMemory(device,b.memory,nullptr);
      }
      if(fence) vkDestroyFence(device,fence,nullptr);
      if(pool) vkDestroyCommandPool(device,pool,nullptr);
      vkDestroyDevice(device,nullptr);
    }
    if(instance) vkDestroyInstance(instance,nullptr);
  }
};
} // namespace acv::readback_test
