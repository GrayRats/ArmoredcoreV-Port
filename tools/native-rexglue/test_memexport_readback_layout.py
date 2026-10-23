#!/usr/bin/env python3
"""Compile the SDK's actual MEMEXPORT range validation and layout-key methods."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp").read_text()

def method(start: str, end: str) -> str:
    a = source.index(start)
    b = source.index(end, a)
    return source[a:b]

validation = method(
    "bool VulkanCommandProcessor::ValidateMemexportReadbackRanges(",
    "void VulkanCommandProcessor::DeferReadbackBufferDestruction(",
)
layout = method(
    "VulkanCommandProcessor::ReadbackBuffer*\nVulkanCommandProcessor::GetOrCreateMemexportReadbackBuffer(",
    "bool VulkanCommandProcessor::IssueDraw_MemexportReadbackFastPath(",
).replace("VulkanCommandProcessor::ReadbackBuffer*", "ReadbackBuffer*")

harness = r'''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <unordered_map>
#include <vector>
#define REXGPU_ERROR(...) ((void)0)
namespace draw_util {
struct MemExportRange { uint32_t base_address_dwords, size_bytes; };
}
struct SharedMemory { static constexpr uint32_t kBufferSize = 1u << 29; };
struct ReadbackBuffer {
  std::vector<std::pair<uint32_t, uint32_t>> memexport_range_layout;
};
struct VulkanCommandProcessor {
  std::vector<draw_util::MemExportRange> memexport_ranges_;
  std::unordered_map<uint64_t, ReadbackBuffer> memexport_readback_buffers_;
  static uint64_t MakeMemexportReadbackKey(uint32_t first, uint32_t size) {
    return (uint64_t(first) << 32) | size;
  }
  bool ValidateMemexportReadbackRanges(uint32_t total_size) const;
  ReadbackBuffer* GetOrCreateMemexportReadbackBuffer(uint32_t total_size);
};
'''
harness += validation + layout + r'''
int main() {
  VulkanCommandProcessor p;
  // A valid range ending exactly at the 512 MiB physical-memory boundary.
  p.memexport_ranges_ = {{(SharedMemory::kBufferSize / 4) - 1, 4}};
  assert(p.ValidateMemexportReadbackRanges(4));
  assert(p.GetOrCreateMemexportReadbackBuffer(4));

  // The same legacy key (first address + packed size) must not alias a
  // differently segmented packed copy.
  p.memexport_ranges_ = {{100, 1}, {101, 7}};
  ReadbackBuffer* first = p.GetOrCreateMemexportReadbackBuffer(8);
  p.memexport_ranges_ = {{100, 4}, {200, 4}};
  ReadbackBuffer* second = p.GetOrCreateMemexportReadbackBuffer(8);
  assert(first && second && first != second);
  assert(p.memexport_readback_buffers_.size() == 3);
  assert(p.GetOrCreateMemexportReadbackBuffer(8) == second);

  p.memexport_ranges_ = {{SharedMemory::kBufferSize / 4, 1}};
  assert(!p.ValidateMemexportReadbackRanges(1));
  assert(!p.GetOrCreateMemexportReadbackBuffer(1));
  p.memexport_ranges_ = {{100, 4}, {200, 4}};
  assert(!p.ValidateMemexportReadbackRanges(7));
  assert(!p.GetOrCreateMemexportReadbackBuffer(7));
  std::puts("PASS: physical range bounds and exact MEMEXPORT staging layouts");
}
'''

with tempfile.TemporaryDirectory(prefix="acv-memexport-layout-") as directory:
    cpp = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    cpp.write_text(harness)
    subprocess.run(
        ["clang++", "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined", str(cpp), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
