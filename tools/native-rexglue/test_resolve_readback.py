#!/usr/bin/env python3
"""Compile the actual SDK Resolve publication tail against controlled Vulkan failures.
This verifies CPU publication/slot contracts, not GPU execution or scene parity.
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
s = (root / "tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp").read_text()
a = s.index("bool VulkanCommandProcessor::AwaitReadbackSubmission(")
b = s.index("bool VulkanCommandProcessor::IssueCopy_ReadbackResolvePath()", a)
wait = s[a:b]
a = s.index("  readback.submission_written[write_index] = GetCurrentSubmission();", b)
b = s.index("\nvoid VulkanCommandProcessor::EvictOldReadbackBuffers", a)
tail = s[a:b]
harness = r'''#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vulkan/vulkan_core.h>
#define REXGPU_ERROR(...) ((void)0)
enum class ReadbackResolveMode {kFull,kFast,kSome};
struct ReadbackBuffer {
 VkBuffer buffers[2]={(VkBuffer)1,(VkBuffer)2};
 VkDeviceMemory memories[2]={(VkDeviceMemory)1,(VkDeviceMemory)2};
 std::array<uint8_t,16> data[2];
 void* mapped_data[2]={data[0].data(),data[1].data()};
 uint32_t sizes[2]={16,16},written_size[2]={0,0};
 uint64_t submission_written[2]={0,0};
 uint32_t current_index=0;
 ReadbackBuffer(){ data[0].fill(0x42);data[1].fill(0x71); }
};
struct Memory {
 std::array<uint8_t,16> data{};
 uint8_t* TranslatePhysical(uint32_t a){assert(a==4096);return data.data();}
};
struct Functions {
 VkResult result=VK_SUCCESS;
 int calls=0;
 VkResult vkInvalidateMappedMemoryRanges(VkDevice,uint32_t n,const VkMappedMemoryRange* r){
  assert(n==1 && r->offset==0 && r->size==VK_WHOLE_SIZE);++calls;return result;
 }
};
struct VulkanCommandProcessor {
 Memory memory;Memory* memory_=&memory;
 Functions dfn;VkDevice device=(VkDevice)1;
 bool device_lost_=false,wait_fails=false,lose_on_poll=false;
 uint64_t submission_completed_=0,current=10,last_wait=0;
 int waits=0;
 uint64_t GetCurrentSubmission(){return current;}
 void CheckSubmissionFenceAndDeviceLoss(uint64_t ticket){
  if(lose_on_poll)device_lost_=true;
  if(ticket){++waits;last_wait=ticket;if(!wait_fails&&!device_lost_)submission_completed_=ticket;}
 }
 bool AwaitReadbackSubmission(uint64_t);
 bool Publish(ReadbackBuffer& readback, uint32_t write_index,ReadbackResolveMode readback_mode){
  const uint32_t written_length=16,written_address=4096;
TAIL
};
WAIT
int main(){
 using Mode=ReadbackResolveMode;
 {VulkanCommandProcessor p;ReadbackBuffer r;
  assert(p.Publish(r,0,Mode::kFull));assert(p.last_wait==10);
  assert(p.memory.data==r.data[0] && p.dfn.calls==1 && r.current_index==1);}
 {VulkanCommandProcessor p;ReadbackBuffer r;p.wait_fails=true;
  assert(!p.Publish(r,0,Mode::kFull));assert(p.dfn.calls==0);
  assert(p.memory.data[0]==0 && r.current_index==0);}
 {VulkanCommandProcessor p;ReadbackBuffer r;p.dfn.result=VK_ERROR_MEMORY_MAP_FAILED;
  assert(!p.Publish(r,0,Mode::kFull));assert(p.memory.data[0]==0 && r.current_index==0);}
 {VulkanCommandProcessor p;ReadbackBuffer r;p.lose_on_poll=true;
  assert(!p.Publish(r,0,Mode::kFull));assert(p.dfn.calls==0);}
 {VulkanCommandProcessor p;ReadbackBuffer r;r.submission_written[1]=9;r.written_size[1]=16;
  p.submission_completed_=8;
  assert(p.Publish(r,0,Mode::kFast));assert(p.last_wait==10);
  assert(p.memory.data==r.data[0]);} // allocated but unfinished previous slot
 {VulkanCommandProcessor p;ReadbackBuffer r;r.submission_written[1]=9;r.written_size[1]=16;
  p.submission_completed_=9;
  assert(p.Publish(r,0,Mode::kFast));assert(p.waits==0);
  assert(p.memory.data==r.data[1]);} // explicitly delayed mode, complete slot
 {VulkanCommandProcessor p;ReadbackBuffer r;r.submission_written[1]=9;r.written_size[1]=8;
  p.submission_completed_=9;
  assert(p.Publish(r,0,Mode::kFast));assert(p.last_wait==10);
  assert(p.memory.data==r.data[0]);} // capacity is not initialized byte count
 {VulkanCommandProcessor p;ReadbackBuffer r;
  assert(p.Publish(r,0,Mode::kSome));assert(p.memory.data==r.data[0]);}
 {VulkanCommandProcessor p;ReadbackBuffer r;r.submission_written[1]=9;r.written_size[1]=16;
  p.submission_completed_=9;
  assert(p.Publish(r,0,Mode::kSome));assert(p.dfn.calls==0);}
 {VulkanCommandProcessor p;p.wait_fails=true;
  assert(!p.AwaitReadbackSubmission(9));assert(!p.AwaitReadbackSubmission(0));}
 std::puts("PASS: actual Resolve publication handles pending tickets, stale sizes, wait/invalidate/device failures and delayed-mode policy");
}
'''
harness=harness.replace("TAIL",tail).replace("WAIT",wait)
with tempfile.TemporaryDirectory(prefix="acv-resolve-") as d:
    cpp=Path(d)/"test.cpp"; exe=Path(d)/"test"
    cpp.write_text(harness)
    subprocess.run(["clang++","-std=c++17","-O1","-g","-fsanitize=address,undefined",str(cpp),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
