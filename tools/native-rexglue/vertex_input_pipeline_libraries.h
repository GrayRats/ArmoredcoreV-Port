#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vulkan/vulkan_core.h>
namespace acv {
// Reuse vertex-input interface libraries; shader/output parts remain provided
// at final linking. Unsupported input extensions or a full cache use
// monolithic.
class VertexInputPipelineLibraries {
  VkDevice device_;
  PFN_vkCreateGraphicsPipelines create_;
  PFN_vkDestroyPipeline destroy_;
  std::mutex mutex_;
  std::map<std::string, VkPipeline> libraries_;
  template <class T> static void Add(std::string &key, T value) {
    key.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

public:
  VertexInputPipelineLibraries(VkDevice d, PFN_vkCreateGraphicsPipelines c,
                               PFN_vkDestroyPipeline x)
      : device_(d), create_(c), destroy_(x) {}
  ~VertexInputPipelineLibraries() {
    for (const auto &item : libraries_)
      destroy_(device_, item.second, nullptr);
  }
  bool TryCreate(const VkGraphicsPipelineCreateInfo &original,
                 VkPipelineCache cache, VkPipeline &pipeline,
                 VkResult &result) try {
    pipeline = VK_NULL_HANDLE;
    auto *vi = original.pVertexInputState;
    auto *ia = original.pInputAssemblyState;
    if (!vi || !ia || vi->pNext || ia->pNext || original.flags)
      return false;
    if (original.pDynamicState)
      for (uint32_t i = 0; i < original.pDynamicState->dynamicStateCount; ++i) {
        auto s = original.pDynamicState->pDynamicStates[i];
        if (s == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT ||
            s == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE ||
            s == VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY ||
            s == VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE)
          return false;
      }
    std::string key;
    Add(key, vi->flags);
    Add(key, vi->vertexBindingDescriptionCount);
    Add(key, vi->vertexAttributeDescriptionCount);
    for (uint32_t i = 0; i < vi->vertexBindingDescriptionCount; ++i) {
      auto &b = vi->pVertexBindingDescriptions[i];
      Add(key, b.binding);
      Add(key, b.stride);
      Add(key, b.inputRate);
    }
    for (uint32_t i = 0; i < vi->vertexAttributeDescriptionCount; ++i) {
      auto &a = vi->pVertexAttributeDescriptions[i];
      Add(key, a.location);
      Add(key, a.binding);
      Add(key, a.format);
      Add(key, a.offset);
    }
    Add(key, ia->flags);
    Add(key, ia->topology);
    Add(key, ia->primitiveRestartEnable);
    VkPipeline library = VK_NULL_HANDLE;
    {
      std::lock_guard<std::mutex> guard(mutex_);
      auto it = libraries_.find(key);
      if (it != libraries_.end())
        library = it->second;
      else {
        if (libraries_.size() >= 128)
          return false;
        VkGraphicsPipelineLibraryCreateInfoEXT subset{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};
        subset.flags =
            VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
        VkGraphicsPipelineCreateInfo info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.pNext = &subset;
        info.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        info.pVertexInputState = vi;
        info.pInputAssemblyState = ia;
        info.basePipelineIndex = -1;
        result = create_(device_, cache, 1, &info, nullptr, &library);
        if (result != VK_SUCCESS) {
          if (library)
            destroy_(device_, library, nullptr);
          return true;
        }
        try {
          libraries_.emplace(std::move(key), library);
        } catch (...) {
          destroy_(device_, library, nullptr);
          result = VK_ERROR_OUT_OF_HOST_MEMORY;
          return true;
        }
      }
    }
    VkPipelineLibraryCreateInfoKHR links{
        VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};
    links.pNext = original.pNext;
    links.libraryCount = 1;
    links.pLibraries = &library;
    VkGraphicsPipelineLibraryCreateInfoEXT supplied{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};
    supplied.pNext = &links;
    supplied.flags =
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    VkGraphicsPipelineCreateInfo info = original;
    info.pNext = &supplied;
    info.pVertexInputState = nullptr;
    info.pInputAssemblyState = nullptr;
    result = create_(device_, cache, 1, &info, nullptr, &pipeline);
    return true;
  } catch (...) {
    result = VK_ERROR_OUT_OF_HOST_MEMORY;
    return true;
  }
};
} // namespace acv
