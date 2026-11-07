#include "vulkan_readback_test_support.h"
#include "coherent_readback_publication.h"
#include <cstring>
using namespace acv::readback_test;
namespace {
constexpr size_t kMax=7249920,kBytes=kMax+128;
void Run(bool enabled,unsigned trial) {
 Vulkan vk;vk.Init();vk.Allocate(0,kMax,false);vk.Allocate(1,kBytes,true);
 const auto flags=vk.memory_properties.memoryTypes[vk.buffers[1].type].propertyFlags;
 bool noncoherent_available=false;
 VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(vk.device,vk.buffers[1].handle,&requirements);
 for(uint32_t i=0;i<vk.memory_properties.memoryTypeCount;++i) {
  const auto f=vk.memory_properties.memoryTypes[i].propertyFlags;
  if((requirements.memoryTypeBits&(1u<<i)) && (f&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && !(f&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))noncoherent_available=true;
 }
 std::vector<uint8_t> destination(kBytes,0xCE);unsigned invalidates=0,published=0;
 const auto start=Clock::now();
 for(unsigned generation=0;generation<32;++generation) {
  const size_t lengths[]={4096,3768320,kMax};const size_t length=lengths[generation%3];
  const uint32_t pattern=0x6A000000u+generation;
  vk.Begin();vkCmdFillBuffer(vk.command,vk.buffers[0].handle,0,length,pattern);
  vk.Barrier(vk.buffers[0].handle,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  vkCmdFillBuffer(vk.command,vk.buffers[1].handle,0,kBytes,0xCECECECE);
  vk.Barrier(vk.buffers[1].handle,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferCopy copy{0,64,length};vkCmdCopyBuffer(vk.command,vk.buffers[0].handle,vk.buffers[1].handle,1,&copy);
  vk.Barrier(vk.buffers[1].handle,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT);
  vk.SubmitWait();VK_CHECK(vkGetFenceStatus(vk.device,vk.fence));
  std::fill(destination.begin(),destination.end(),0xCE);
  Require(acv::PublishCompletedReadback(true,enabled,true,flags,[&] {
    ++invalidates;VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=vk.buffers[1].memory;range.size=VK_WHOLE_SIZE;
    return vkInvalidateMappedMemoryRanges(vk.device,1,&range);
   },[&] {
    std::memcpy(destination.data()+64,static_cast<uint8_t*>(vk.buffers[1].mapping)+64,length);++published;return true;
   }),"completed publication failed");
  // Validate both GPU-written prefix/tail guards and host-copy guards. Every
  // generation is checked before reuse; no averaging hides stale copies.
  for(size_t i=0;i<kBytes;i+=4) {
   uint32_t host_word,gpu_word;std::memcpy(&host_word,destination.data()+i,4);std::memcpy(&gpu_word,static_cast<uint8_t*>(vk.buffers[1].mapping)+i,4);
   const uint32_t expected=i>=64 && i<64+length ? pattern:0xCECECECE;
   Require(host_word==expected && gpu_word==expected,"generation or guard mismatch");
  }
 }
 const bool coherent=flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
 Require(published==32 && invalidates==(enabled && coherent?0:32),"invalidate API count mismatch");
 const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 std::cout<<"{\"trial\":"<<trial<<",\"skip_coherent\":"<<(enabled?"true":"false")<<",\"memory_type\":"<<vk.buffers[1].type<<",\"memory_flags\":"<<flags<<",\"invalidate_calls\":"<<invalidates<<",\"generations\":"<<published<<",\"elapsed_ms\":"<<ms<<",\"exact_bytes_guards\":true,\"noncoherent_compatible_type_available\":"<<(noncoherent_available?"true":"false")<<",\"noncoherent_gpu_tested\":false}\n";
}
}
int main() try {
 for(unsigned trial=0;trial<2;++trial)for(unsigned order=0;order<2;++order)Run((trial+order)%2,trial);
 std::cout<<"PASS same-input GPU staging AB/BA:32generations each,actual memory flags,fence+HOST_READ ordering,exact payload/GPU+CPUguards,invalidate APIcounts;noncoherent injected host coverage only\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
