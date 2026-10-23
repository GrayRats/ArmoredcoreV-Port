#!/usr/bin/env python3
"""Compile the SDK's actual readback eviction method against Vulkan lifetime mocks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp").read_text()
start = source.index("void VulkanCommandProcessor::EvictOldReadbackBuffers(")
end = source.index("\nbool VulkanCommandProcessor::InitializeOcclusionQueryResources()", start)
method = source[start:end]
assert source.count("readback.eviction_pending = false;") == 2, (
    "both Resolve and MEMEXPORT reuse paths must cancel stale eviction requests"
)

harness = r'''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

struct VulkanDevice {
  struct Functions {
    mutable int unmaps = 0, destroys = 0, frees = 0;
    void vkUnmapMemory(VkDevice, VkDeviceMemory) const { ++unmaps; }
    void vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) const { ++destroys; }
    void vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) const { ++frees; }
  } f;
  const Functions& functions() const { return f; }
  VkDevice device() const { return (VkDevice)1; }
};
namespace ui { namespace vulkan { using VulkanDevice = ::VulkanDevice; } }
struct ReadbackBuffer {
  VkBuffer buffers[2] = {(VkBuffer)1, (VkBuffer)2};
  VkDeviceMemory memories[2] = {(VkDeviceMemory)1, (VkDeviceMemory)2};
  void* mapped_data[2] = {(void*)1, (void*)2};
  uint32_t sizes[2] = {4096, 4096};
  uint64_t submission_written[2] = {0, 0};
  uint32_t written_size[2] = {0, 0};
  uint64_t last_used_frame = 0;
  bool eviction_pending = false;
};
constexpr size_t kMaxReadbackBuffers = 256;
constexpr uint64_t kReadbackBufferEvictionAgeFrames = 60;
struct VulkanCommandProcessor {
  uint64_t frame_current_ = 100, submission_completed_ = 4;
  VulkanDevice vk;
  VulkanDevice* GetVulkanDevice() { return &vk; }
  void EvictOldReadbackBuffers(std::unordered_map<uint64_t, ReadbackBuffer>&);
};
'''
harness += method
harness += r'''
int main() {
  using Map = std::unordered_map<uint64_t, ReadbackBuffer>;
  {
    VulkanCommandProcessor p; Map m; auto& r = m[1];
    r.last_used_frame = 0; r.submission_written[0] = 5;
    p.EvictOldReadbackBuffers(m);
    assert(m.size() == 1 && p.vk.f.unmaps == 0 && p.vk.f.destroys == 0 && p.vk.f.frees == 0);
    p.submission_completed_ = 5;
    p.EvictOldReadbackBuffers(m);
    assert(m.empty() && p.vk.f.unmaps == 2 && p.vk.f.destroys == 2 && p.vk.f.frees == 2);
  }
  {
    VulkanCommandProcessor p; Map m;
    for (uint64_t i = 0; i <= kMaxReadbackBuffers; ++i) {
      m[i].last_used_frame = p.frame_current_;
    }
    m[0].submission_written[1] = 9;
    m[0].last_used_frame = 0;
    p.EvictOldReadbackBuffers(m);
    assert(m.size() == kMaxReadbackBuffers);
    assert(m.count(0) == 1 && m[0].buffers[0] == (VkBuffer)1);
    assert(p.vk.f.destroys == 2);
    p.submission_completed_ = 9;
    p.EvictOldReadbackBuffers(m);
    assert(m.size() == kMaxReadbackBuffers - 1);
    assert(m.count(0) == 0 && p.vk.f.destroys == 4);
  }
  std::puts("PASS: actual SDK eviction defers pending slots and reclaims them after completion");
}
'''

with tempfile.TemporaryDirectory(prefix="acv-readback-eviction-") as directory:
    cpp = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    cpp.write_text(harness)
    subprocess.run(["clang++", "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
                    str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
