#!/usr/bin/env python3
"""Compile actual Vulkan staging teardown against fault-injected fence/device APIs."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/'tools/rexglue-sdk/src/graphics/vulkan/command_processor.cpp').read_text()
a=source.index('void VulkanCommandProcessor::ShutdownContext() {')
b=source.index('  AwaitAllQueueOperationsCompletion();\n  InvalidateAllVertexBufferResidency();',a)
body=source[a:b]+'}\n'
harness=r"""
#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>
namespace rex { [[noreturn]] inline void FatalError(const char*) {throw std::runtime_error("unsafe teardown");} }
static std::vector<int> events;
struct Functions {
 void vkUnmapMemory(int,int) const {events.push_back(5);}
 void vkDestroyBuffer(int,int,void*) const {events.push_back(6);}
 void vkFreeMemory(int,int,void*) const {events.push_back(7);}
};
struct Device {Functions f;const Functions& functions() const {return f;}int device()const{return 1;}};
struct Bridge {
 bool drained=true;
 bool Drain(){events.push_back(1);return drained;}
 void Close(){events.push_back(2);}
 bool CancelAfterDeviceStopped(){events.push_back(4);return true;}
};
struct Slot {int memory=1,buffer=1;};
struct Context {Bridge bridge;std::vector<std::unique_ptr<Slot>> slots;};
struct VulkanCommandProcessor {
 std::unique_ptr<Context> general_readback_;
 Device vd;bool device_lost_=false,completed=true;
 const Device* GetVulkanDevice(){return &vd;}
 bool AwaitAllQueueOperationsCompletion(){events.push_back(3);return completed;}
 void ShutdownContext();
};
"""
tests=r"""
int main(){
 for(bool drained:{false,true}) for(bool completed:{false,true}) for(bool lost:{false,true}) {
  events.clear();VulkanCommandProcessor cp;
  cp.general_readback_=std::make_unique<Context>();cp.general_readback_->bridge.drained=drained;
  cp.general_readback_->slots.push_back(std::make_unique<Slot>());
  cp.completed=completed;cp.device_lost_=lost;bool failed=false;
  try{cp.ShutdownContext();}catch(const std::runtime_error&){failed=true;}
  if(!completed && !lost){assert(failed && events==std::vector<int>({1,2,3}));assert(cp.general_readback_);}
  else {assert(!failed && events==std::vector<int>({1,2,3,4,5,6,7}));assert(!cp.general_readback_);}
 }
 events.clear();VulkanCommandProcessor empty;empty.ShutdownContext();assert(events.empty());
}
"""
with tempfile.TemporaryDirectory(prefix='acv-staging-teardown-') as name:
 p=Path(name);(p/'test.cpp').write_text(harness+body+tests)
 subprocess.run(['clang++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run(['timeout','20s',str(p/'test')],check=True)
print('PASS actual Vulkan pool teardown: all8 drain/fence/device-loss cases, including empty bridge with uncommitted inflight copy; no frees before completion; ASan/UBSan')
