#!/usr/bin/env python3
"""Check deferred staging destruction on failed MEMEXPORT completion waits."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp").read_text()
start = source.index("void VulkanCommandProcessor::DeferReadbackBufferDestruction(")
end = source.index("bool VulkanCommandProcessor::IssueDraw_MemexportReadbackFullPath(", start)
method = source[start:end]

full_start = source.index("bool VulkanCommandProcessor::IssueDraw_MemexportReadbackFullPath(")
fast_start = source.index("bool VulkanCommandProcessor::IssueDraw_MemexportReadbackFastPath(", full_start)
full_path = source[full_start:fast_start]
wait_failure = full_path[full_path.index("if (!AwaitAllQueueOperationsCompletion()) {"):]
wait_failure = wait_failure[:wait_failure.index("  }") + 4]
assert "DeferReadbackBufferDestruction(readback_submission, readback_buffer, readback_memory)" in wait_failure
assert "vkDestroyBuffer" not in wait_failure and "vkFreeMemory" not in wait_failure

fast_path = source[fast_start:source.index("bool VulkanCommandProcessor::IssueCopy()", fast_start)]
assert "if (!IssueDraw_MemexportReadbackFullPath(total_size))" in fast_path
assert "readback.current_index = read_index;" in fast_path
draw_start = source.index("bool VulkanCommandProcessor::IssueDraw(")
draw_end = source.index("bool VulkanCommandProcessor::ValidateMemexportReadbackRanges(", draw_start)
draw = source[draw_start:draw_end]
assert "if (!IssueDraw_MemexportReadbackFastPath(memexport_total_size))" in draw
assert "if (!IssueDraw_MemexportReadbackFullPath(memexport_total_size))" in draw

harness = r'''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <utility>
#include <vulkan/vulkan_core.h>
struct VulkanCommandProcessor {
  std::deque<std::pair<uint64_t, VkBuffer>> destroy_buffers_;
  std::deque<std::pair<uint64_t, VkDeviceMemory>> destroy_memory_;
  void DeferReadbackBufferDestruction(uint64_t, VkBuffer, VkDeviceMemory);
};
'''
harness += method + r'''
int main() {
  VulkanCommandProcessor p;
  const uint64_t pending_submission = 37;
  const VkBuffer buffer = (VkBuffer)0x1234;
  const VkDeviceMemory memory = (VkDeviceMemory)0x5678;
  p.DeferReadbackBufferDestruction(pending_submission, buffer, memory);
  assert(p.destroy_buffers_.size() == 1 && p.destroy_memory_.size() == 1);
  assert(p.destroy_buffers_.front().first == pending_submission);
  assert(p.destroy_buffers_.front().second == buffer);
  assert(p.destroy_memory_.front().first == pending_submission);
  assert(p.destroy_memory_.front().second == memory);
  std::puts("PASS: failed-wait staging handles stay owned by their submission ticket");
}
'''

with tempfile.TemporaryDirectory(prefix="acv-memexport-failure-") as directory:
    cpp = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    cpp.write_text(harness)
    subprocess.run(
        ["clang++", "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined", str(cpp), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
