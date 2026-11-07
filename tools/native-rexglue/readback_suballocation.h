#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace acv {
class ReadbackSuballocation {
  static constexpr VkDeviceSize kBudget = 64ULL << 20;
  VkDevice device_;
  VkBuffer buffer_{};
  VkDeviceMemory memory_{};
  void *mapping_{};
  uint32_t type_ = UINT32_MAX;
  VkMemoryPropertyFlags flags_{};
  VkDeviceSize atom_ = 4, allocated_ = 0;
  bool used_ = false;
  PFN_vkDestroyBuffer destroy_buffer_{};
  PFN_vkFreeMemory free_{};
  PFN_vkUnmapMemory unmap_{};
  struct Range {
    uint64_t id = 0;
    VkDeviceSize offset = 0, size = 0;
  };
  std::array<Range, 64> ranges_{};
  bool Gap(VkDeviceSize bytes, VkDeviceSize &offset, VkDeviceSize &span) const {
    if (!bytes || bytes > kBudget || atom_ > kBudget)
      return false;
    span = (bytes + atom_ - 1) & ~(atom_ - 1);
    auto sorted = ranges_;
    std::sort(sorted.begin(), sorted.end(), [](const Range &a, const Range &b) {
      return a.size && (!b.size || a.offset < b.offset);
    });
    offset = 0;
    for (const auto &r : sorted) {
      if (!r.size)
        break;
      if (r.offset >= offset && r.offset - offset >= span)
        return true;
      offset = r.offset + r.size;
    }
    return offset <= kBudget && span <= kBudget - offset;
  }
  void Cleanup() {
    if (mapping_)
      unmap_(device_, memory_);
    if (buffer_)
      destroy_buffer_(device_, buffer_, nullptr);
    if (memory_)
      free_(device_, memory_, nullptr);
    mapping_ = nullptr;
    buffer_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
  }

public:
  VkResult result = VK_SUCCESS;
  ReadbackSuballocation(VkDevice device, PFN_vkGetDeviceProcAddr proc,
                        const VkPhysicalDeviceMemoryProperties &types,
                        VkDeviceSize atom)
      : device_(device), atom_(std::max<VkDeviceSize>(4, atom)) {
    auto create =
        reinterpret_cast<PFN_vkCreateBuffer>(proc(device, "vkCreateBuffer"));
    auto requirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
        proc(device, "vkGetBufferMemoryRequirements"));
    auto allocate = reinterpret_cast<PFN_vkAllocateMemory>(
        proc(device, "vkAllocateMemory"));
    auto bind = reinterpret_cast<PFN_vkBindBufferMemory>(
        proc(device, "vkBindBufferMemory"));
    auto map = reinterpret_cast<PFN_vkMapMemory>(proc(device, "vkMapMemory"));
    destroy_buffer_ =
        reinterpret_cast<PFN_vkDestroyBuffer>(proc(device, "vkDestroyBuffer"));
    free_ = reinterpret_cast<PFN_vkFreeMemory>(proc(device, "vkFreeMemory"));
    unmap_ = reinterpret_cast<PFN_vkUnmapMemory>(proc(device, "vkUnmapMemory"));
    if (!create || !requirements || !allocate || !bind || !map ||
        !destroy_buffer_ || !free_ || !unmap_) {
      result = VK_ERROR_INITIALIZATION_FAILED;
      return;
    }
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = kBudget;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = create(device_, &info, nullptr, &buffer_);
    if (result != VK_SUCCESS) {
      buffer_ = VK_NULL_HANDLE;
      return;
    }
    VkMemoryRequirements req{};
    requirements(device_, buffer_, &req);
    if (req.size > kBudget) {
      result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
      Cleanup();
      return;
    }
    int best = -1;
    for (uint32_t i = 0; i < types.memoryTypeCount; ++i) {
      auto f = types.memoryTypes[i].propertyFlags;
      if (!(req.memoryTypeBits & (1u << i)) ||
          !(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        continue;
      int score = ((f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 4 : 0) +
                  ((f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 2 : 0);
      if (score > best) {
        best = score;
        type_ = i;
        flags_ = f;
      }
    }
    if (type_ == UINT32_MAX) {
      result = VK_ERROR_FEATURE_NOT_PRESENT;
      Cleanup();
      return;
    }
    VkMemoryDedicatedAllocateInfo dedicated{
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    dedicated.buffer = buffer_;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.pNext = &dedicated;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = type_;
    result = allocate(device_, &ai, nullptr, &memory_);
    if (result != VK_SUCCESS) {
      memory_ = VK_NULL_HANDLE;
      Cleanup();
      return;
    }
    result = bind(device_, buffer_, memory_, 0);
    if (result != VK_SUCCESS) {
      Cleanup();
      return;
    }
    result = map(device_, memory_, 0, VK_WHOLE_SIZE, 0, &mapping_);
    if (result != VK_SUCCESS) {
      mapping_ = nullptr;
      Cleanup();
      return;
    }
    allocated_ = req.size;
  }
  ~ReadbackSuballocation() {
    if (!used_)
      Cleanup();
  }
  ReadbackSuballocation(const ReadbackSuballocation &) = delete;
  ReadbackSuballocation &operator=(const ReadbackSuballocation &) = delete;
  bool ready() const { return mapping_ != nullptr; }
  bool Acquire(uint64_t id, VkDeviceSize bytes, VkDeviceSize &offset,
               VkDeviceSize &span) {
    if (!ready() || !id || !Gap(bytes, offset, span))
      return false;
    for (auto &r : ranges_)
      if (!r.size) {
        r = {id, offset, span};
        used_ = true;
        return true;
      }
    return false;
  }
  void Release(uint64_t id) {
    for (auto &r : ranges_)
      if (r.id == id) {
        r = {};
        return;
      }
  }
  void DestroyAfterDeviceStopped() {
    Cleanup();
    ranges_ = {};
    used_ = false;
  }
  VkBuffer buffer() const { return buffer_; }
  VkDeviceMemory memory() const { return memory_; }
  void *data(VkDeviceSize offset) const {
    return static_cast<uint8_t *>(mapping_) + offset;
  }
  uint32_t type() const { return type_; }
  VkMemoryPropertyFlags flags() const { return flags_; }
  VkDeviceSize allocated() const { return allocated_; }
};
} // namespace acv
