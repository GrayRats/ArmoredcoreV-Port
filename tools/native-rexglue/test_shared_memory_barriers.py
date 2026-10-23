#!/usr/bin/env python3
"""Exercise actual SDK Use/GetUsageMasks bodies against a recording processor.
This is a host access-scope regression, not a GPU execution or rendering oracle.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root / "tools/rexglue-sdk/src/graphics/vulkan/shared_memory.cpp")
args = parser.parse_args()
source = args.source.read_text()

def method(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

header = (root / "tools/rexglue-sdk/include/rex/graphics/vulkan/shared_memory.h").read_text()
enum_start = header.index("enum class Usage")
enum_end = header.index("};", enum_start) + 2
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>
#include <cstdio>
#include <vulkan/vulkan_core.h>
#define assert_true(x) assert(x)
#define assert_unhandled_case(x) assert(false)
struct Barrier { VkDeviceSize offset,size; VkPipelineStageFlags src_stage,dst_stage; VkAccessFlags src,dst; };
struct Processor {
 std::vector<Barrier> barriers;
 void PushBufferMemoryBarrier(VkBuffer, VkDeviceSize offset, VkDeviceSize size,
 VkPipelineStageFlags ss,VkPipelineStageFlags ds,VkAccessFlags sa,VkAccessFlags da,
 uint32_t,uint32_t,bool) { barriers.push_back({offset,size,ss,ds,sa,da}); }
};
class VulkanSharedMemory {
public:
 ENUM
 static constexpr uint32_t kBufferSize=1u<<29;
 Processor command_processor_;
 VkPipelineStageFlags guest_shader_pipeline_stages_=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
 VkBuffer buffer_=VK_NULL_HANDLE;
 Usage last_usage_=Usage::kRead;
 std::pair<uint32_t,uint32_t> last_written_range_={};
 void Use(Usage,std::pair<uint32_t,uint32_t> = {});
 void GetUsageMasks(Usage,VkPipelineStageFlags&,VkAccessFlags&) const;
};
METHODS
int main() {
 using U=VulkanSharedMemory::Usage;
 VulkanSharedMemory m;
 m.Use(U::kComputeWrite,{4096,8192});
 assert(m.command_processor_.barriers.size()==1);
 auto b=m.command_processor_.barriers.back();
 assert(b.dst_stage==VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
 assert(b.dst & VK_ACCESS_SHADER_WRITE_BIT);
 m.Use(U::kRead);
 b=m.command_processor_.barriers.back();
 assert(b.src & VK_ACCESS_SHADER_WRITE_BIT);
 assert(b.dst & VK_ACCESS_SHADER_READ_BIT);
 assert(b.dst & VK_ACCESS_TRANSFER_READ_BIT);
 auto count=m.command_processor_.barriers.size();
 m.Use(U::kRead);
 assert(m.command_processor_.barriers.size()==count);
 m.Use(U::kComputeWrite,{4096,8192});
 m.Use(U::kComputeWrite,{32768,4096});
 b=m.command_processor_.barriers.back();
 assert(b.offset==4096 && b.size==8192);
 assert((b.src & VK_ACCESS_SHADER_WRITE_BIT) && (b.dst & VK_ACCESS_SHADER_WRITE_BIT));
 m.Use(U::kTransferDestination,{0,4096});
 m.Use(U::kRead);
 b=m.command_processor_.barriers.back();
 assert(b.src & VK_ACCESS_TRANSFER_WRITE_BIT);
 assert(b.dst & VK_ACCESS_SHADER_READ_BIT);
 std::puts("PASS: compute-to-read, compute-to-compute, transfer-to-read and unchanged-read barriers");
}
'''
harness = harness.replace("ENUM", header[enum_start:enum_end]).replace("METHODS", "\n".join([
    method("void VulkanSharedMemory::Use("), method("void VulkanSharedMemory::GetUsageMasks(")]))
with tempfile.TemporaryDirectory(prefix="acv-sync-") as td:
    cpp=Path(td)/"test.cpp"
    exe=Path(td)/"test"
    cpp.write_text(harness)
    subprocess.run(["clang++","-std=c++17","-O1","-g","-fsanitize=undefined","-I/home/art/.local/opt/rexglue-sdk/0.10.0/include",str(cpp),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
