// Real timestamp-ring oracle: actual compute and transfer work, exact snapshots.
// Synthetic CPU ages exercise sampling without a 120-second idle wait.
#include "vulkan_readback_test_support.h"
#include "resolve_descriptor_cache.h"
#include "gpu_timestamp_profiler.h"
#include "buffer_bind_cache.h"
#include <cstring>
#include <fstream>
#include <memory>
using namespace acv::readback_test;
using Cache=acv::ResolveDescriptorCache;
namespace {
using Profiler=acv::GpuTimestampProfiler;
uint64_t query_reads=0;PFN_vkGetQueryPoolResults query_native=nullptr;PFN_vkCmdWriteTimestamp write_native=nullptr;
VKAPI_ATTR VkResult VKAPI_CALL QueryRead(VkDevice d,VkQueryPool p,uint32_t f,uint32_t n,size_t size,void* data,VkDeviceSize stride,VkQueryResultFlags flags){
 Require(!(flags&VK_QUERY_RESULT_WAIT_BIT),"profiler attempted waiting query read");++query_reads;
 return query_native(d,p,f,n,size,data,stride,flags);
}
struct Counters {
 uint64_t allocations=0,updates=0,pools=0;
 static Counters* active;
 static VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice d,const VkDescriptorPoolCreateInfo* i,const VkAllocationCallbacks* a,VkDescriptorPool* p){++active->pools;return vkCreateDescriptorPool(d,i,a,p);}
 static VKAPI_ATTR VkResult VKAPI_CALL Allocate(VkDevice d,const VkDescriptorSetAllocateInfo* i,VkDescriptorSet* p){++active->allocations;return vkAllocateDescriptorSets(d,i,p);}
 static VKAPI_ATTR void VKAPI_CALL Update(VkDevice d,uint32_t n,const VkWriteDescriptorSet* w,uint32_t c,const VkCopyDescriptorSet* p){active->updates+=n;vkUpdateDescriptorSets(d,n,w,c,p);}
 Cache::Api api(){active=this;return {Create,Allocate,Update,vkDestroyDescriptorPool};}
};Counters* Counters::active=nullptr;
struct Resources {
 Vulkan& vk;
 VkDescriptorSetLayout layout{};VkPipelineLayout pipeline_layout{};
 VkShaderModule module{};VkPipeline pipeline{};VkDescriptorPool transient{};
 std::array<VkFence,8> fences{};std::unique_ptr<Cache> cache;std::unique_ptr<Profiler> profiler;
 ~Resources() {
  if(vk.pending) {std::cerr<<"Oracle cannot prove GPU stop; retaining pending resources until process teardown\n";std::_Exit(2);}
  if(profiler)profiler->DestroyAfterDeviceStopped(true);
  if(cache) cache->DestroyAfterDeviceStopped(true);
  if(transient)vkDestroyDescriptorPool(vk.device,transient,nullptr);
  for(auto fence:fences)if(fence)vkDestroyFence(vk.device,fence,nullptr);
  if(pipeline)vkDestroyPipeline(vk.device,pipeline,nullptr);
  if(module)vkDestroyShaderModule(vk.device,module,nullptr);
  if(pipeline_layout)vkDestroyPipelineLayout(vk.device,pipeline_layout,nullptr);
  if(layout)vkDestroyDescriptorSetLayout(vk.device,layout,nullptr);
 }
};
struct Result {uint64_t updates,allocations;};
Result Run(bool enabled,unsigned trial,const std::vector<uint32_t>& code) {
 Vulkan vk;vk.Init();Resources resources{vk};Counters counters;counters.api();
 const VkDeviceSize alignment=std::max<VkDeviceSize>(256,vk.properties.limits.minStorageBufferOffsetAlignment);
 const VkDeviceSize buffer_bytes=alignment*32;
 Require(buffer_bytes<=1<<20,"unexpected alignment exceeds bounded oracle");
 constexpr unsigned kDispatches=32,kPerSubmit=4;
 // Device-local storage buffer. Host-visible snapshots use the SDK readback
 // memory-type preference in the shared fixture, invalidated after fence wait.
 auto& storage=vk.buffers[0];VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
 bi.size=buffer_bytes;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 VK_CHECK(vkCreateBuffer(vk.device,&bi,nullptr,&storage.handle));
 VkMemoryRequirements req{};vkGetBufferMemoryRequirements(vk.device,storage.handle,&req);
 uint32_t type=UINT32_MAX;
 for(uint32_t i=0;i<vk.memory_properties.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) && (vk.memory_properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}
 Require(type!=UINT32_MAX,"no device-local storage memory");
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=type;
 VK_CHECK(vkAllocateMemory(vk.device,&ai,nullptr,&storage.memory));VK_CHECK(vkBindBufferMemory(vk.device,storage.handle,storage.memory,0));
 vk.Allocate(1,buffer_bytes*kDispatches,true);
 VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
 VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&binding;
 auto& layout=resources.layout;VK_CHECK(vkCreateDescriptorSetLayout(vk.device,&li,nullptr,&layout));
 VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,12};
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&layout;pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&push;
 auto& pipeline_layout=resources.pipeline_layout;VK_CHECK(vkCreatePipelineLayout(vk.device,&pli,nullptr,&pipeline_layout));
 VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};smi.codeSize=code.size()*4;smi.pCode=code.data();
 auto& module=resources.module;VK_CHECK(vkCreateShaderModule(vk.device,&smi,nullptr,&module));
 VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pci.layout=pipeline_layout;pci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pci.stage.module=module;pci.stage.pName="main";
 auto& pipeline=resources.pipeline;VK_CHECK(vkCreateComputePipelines(vk.device,VK_NULL_HANDLE,1,&pci,nullptr,&pipeline));
 resources.cache=std::make_unique<Cache>(counters.api(),vk.device,layout,storage.handle,buffer_bytes,vk.properties.limits.maxStorageBufferRange,vk.properties.limits.minStorageBufferOffsetAlignment);
 auto& cache=*resources.cache;
 auto& transient=resources.transient;
 if(false){VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,kDispatches};VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=kDispatches;pi.poolSizeCount=1;pi.pPoolSizes=&ps;VK_CHECK(Counters::Create(vk.device,&pi,nullptr,&transient));}
 std::vector<uint8_t> expected(buffer_bytes,0xCE),snapshots;snapshots.reserve(buffer_bytes*kDispatches);
 std::array<VkDescriptorSet,4> immutable_sets{};
 auto& fences=resources.fences;
 uint32_t family_count=0;vkGetPhysicalDeviceQueueFamilyProperties(vk.physical,&family_count,nullptr);
 std::vector<VkQueueFamilyProperties> families(family_count);vkGetPhysicalDeviceQueueFamilyProperties(vk.physical,&family_count,families.data());
 uint32_t family=0;while(family<family_count && !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))++family;
 Require(family<family_count,"fixture graphics queue unavailable");
 if(enabled){
  query_native=reinterpret_cast<PFN_vkGetQueryPoolResults>(vkGetDeviceProcAddr(vk.device,"vkGetQueryPoolResults"));
  write_native=reinterpret_cast<PFN_vkCmdWriteTimestamp>(vkGetDeviceProcAddr(vk.device,"vkCmdWriteTimestamp"));
  Require(query_native && write_native,"core timestamp PFN missing");
  resources.profiler=std::make_unique<Profiler>(Profiler::Api{vkCreateQueryPool,QueryRead,vkDestroyQueryPool},vk.device,families[family].timestampValidBits,vk.properties.limits.timestampPeriod);
  Require(resources.profiler->enabled(),"timestamps unsupported on selected queue");
  // A genuinely GPU-executed reset with no timestamp must yield NOT_READY and
  // zero availability, before any range is admitted to the shared helper.
  vk.Begin();vkCmdResetQueryPool(vk.command,resources.profiler->pool(),0,32);vk.SubmitWait();
  std::array<Profiler::Value,2> values{};
  const auto status=QueryRead(vk.device,resources.profiler->pool(),0,2,sizeof(values),values.data(),sizeof(Profiler::Value),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  Require(status==VK_NOT_READY && !values[0].available && !values[1].available,"reset queries unexpectedly available");
 }
 std::cout<<"{\"timestampValidBits\":"<<families[family].timestampValidBits<<",\"timestampPeriod_ns\":"<<vk.properties.limits.timestampPeriod<<",\"timestampComputeAndGraphics\":"<<vk.properties.limits.timestampComputeAndGraphics<<"}\n";
 uint64_t collected=0,regions=0,writes=0;double measured_ns=0;uint64_t reads_before=query_reads;
 unsigned submitted=0;auto start=Clock::now();
 for(unsigned group=0;group<8;++group) {
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=vk.pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
  VkCommandBuffer cmd{};VK_CHECK(vkAllocateCommandBuffers(vk.device,&ca,&cmd));
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;VK_CHECK(vkBeginCommandBuffer(cmd,&begin));
  acv::PushConstantCache push_cache;
  auto timestamp=[&](VkPipelineStageFlagBits stage,VkQueryPool pool,uint32_t query){++writes;write_native(cmd,stage,pool,query);};
  if(enabled)Require(resources.profiler->Begin(group+1,120+group*0.1,[&](VkQueryPool pool,uint32_t first,uint32_t count){Require(first==0,"completed slot not reused");vkCmdResetQueryPool(cmd,pool,first,count);}),"sample rejected");
  if(!group) vkCmdFillBuffer(cmd,storage.handle,0,buffer_bytes,0xCECECECE);
  for(unsigned local=0;local<kPerSubmit;++local) {
   const unsigned index=group*kPerSubmit+local,key=index%4;
   // Same offset with two ranges, and nonzero offsets with equal ranges.
   const VkDeviceSize offset=alignment*(key==0 || key==1?2:key==2?8:16);
   const VkDeviceSize range=key==1?256:128;
   VkDescriptorBufferInfo info{storage.handle,offset,range};VkDescriptorSet set{};
   if(true) {
    auto result=cache.Get(info);Require(result.status==Cache::Status::Ready,"cache lookup failed");set=result.set;
    if(index>=4) Require(result.hit && set==immutable_sets[key],"in-flight key was not reused immutably");
    else immutable_sets[key]=set;
   } else {
    VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=transient;da.descriptorSetCount=1;da.pSetLayouts=&layout;
    VK_CHECK(Counters::Allocate(vk.device,&da,&set));
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&info;
    Counters::Update(vk.device,1,&write,0,nullptr);
   }
   VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=storage.handle;barrier.size=VK_WHOLE_SIZE;
   barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
   // Preserve runtime subtraction: shader destination is absolute minus exact
   // descriptor offset. Exercise a nonzero relative base and both side guards.
   const uint32_t absolute=uint32_t(offset)+16;
   uint32_t constants[]={absolute-uint32_t(info.offset),uint32_t((range-32)/4),0x55000000u+index};
   vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
   vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,nullptr);
   Require(push_cache.Update(pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants),"changed push dropped");
   vkCmdPushConstants(cmd,pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants);
   Require(!push_cache.Update(pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants),"duplicate push retained");
   if(enabled)resources.profiler->Mark(group+1,2,timestamp);
   vkCmdDispatch(cmd,1,1,1);
   barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
   if(enabled)resources.profiler->Mark(group+1,3,timestamp,buffer_bytes);
   VkBufferCopy copy{0,buffer_bytes*index,buffer_bytes};vkCmdCopyBuffer(cmd,storage.handle,vk.buffers[1].handle,1,&copy);
   for(uint32_t word=0;word<constants[1];++word){uint32_t value=constants[2]^word;std::memcpy(expected.data()+absolute+word*4,&value,4);}
   snapshots.insert(snapshots.end(),expected.begin(),expected.end());
  }
  VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=vk.buffers[1].handle;host.offset=group*kPerSubmit*buffer_bytes;host.size=kPerSubmit*buffer_bytes;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
  if(enabled)resources.profiler->Seal(group+1,timestamp);
  VK_CHECK(vkEndCommandBuffer(cmd));VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK_CHECK(vkCreateFence(vk.device,&fi,nullptr,&fences[group]));
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
  VK_CHECK(vkQueueSubmit(vk.queue,1,&submit,fences[group]));vk.pending=true;++submitted;
  Require(!cache.DestroyAfterDeviceStopped(false),"cache destroyed without stop proof");
  if(enabled){resources.profiler->Submitted(group+1);const auto before=query_reads;
    resources.profiler->Collect(group,120+group*0.1+0.01,[](auto&,auto&){Require(false,"premature query read");});Require(query_reads==before,"read before same-ticket completion");}
  VK_CHECK(vkWaitForFences(vk.device,1,&fences[group],VK_TRUE,5'000'000'000ULL));VK_CHECK(vkGetFenceStatus(vk.device,fences[group]));vk.pending=false;
  if(enabled){
   VK_CHECK(resources.profiler->Collect(group+1,121+group*0.1,[&](const auto& record,const auto& ns){
    ++collected;Require(record.ticket==group+1 && record.regions==8 && !record.truncated,"timestamp metadata mismatch");
    for(unsigned i=0;i<record.regions;++i){Require(record.spans[i].category==(i%2?3:2)&&record.spans[i].commands==1,"category ordering mismatch");Require(record.spans[i].copy_bytes==(i%2?buffer_bytes:0),"copy byte metadata mismatch");Require(std::isfinite(ns[i])&&ns[i]>=0,"invalid duration");++regions;measured_ns+=ns[i];}
   }));Require(!resources.profiler->occupied(),"completed query slot not released");
  }

 }
 VK_CHECK(vkWaitForFences(vk.device,submitted,fences.data(),VK_TRUE,5'000'000'000ULL));vk.pending=false;
 for(auto fence:fences) VK_CHECK(vkGetFenceStatus(vk.device,fence));
 VkMappedMemoryRange mapped{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};mapped.memory=vk.buffers[1].memory;mapped.size=VK_WHOLE_SIZE;
 VK_CHECK(vkInvalidateMappedMemoryRanges(vk.device,1,&mapped));
 Require(std::memcmp(vk.buffers[1].mapping,snapshots.data(),snapshots.size())==0,"descriptor snapshot or prefix/middle/tail guard mismatch");
 double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 Require(counters.updates==4 && counters.allocations==4,"unexpected descriptor API count");
 Require(cache.DestroyAfterDeviceStopped(true),"pool teardown failed");
 Require(!enabled || (collected==8 && regions==64 && writes==128 && query_reads-reads_before==8),"timestamp counters mismatch");
 std::cout<<"{\"trial\":"<<trial<<",\"profile\":"<<(enabled?"true":"false")<<",\"dispatches\":32,\"submissions\":8,\"snapshot_bytes\":"<<snapshots.size()<<",\"elapsed_ms\":"<<ms<<",\"collected\":"<<collected<<",\"regions\":"<<regions<<",\"timestamp_writes\":"<<writes<<",\"query_reads\":"<<query_reads-reads_before<<",\"interval_sum_ns_not_exclusive\":"<<measured_ns<<",\"exact_every_dispatch_and_guards\":true}\n";
 return {counters.updates,counters.allocations};
}
}
int main(int argc,char** argv) try {
 Require(argc==2,"provide compiled oracle SPIR-V path");std::ifstream input(argv[1],std::ios::binary|std::ios::ate);Require(bool(input),"SPIR-V unavailable");
 auto bytes=input.tellg();Require(bytes>0 && uint64_t(bytes)%4==0,"invalid SPIR-V size");std::vector<uint32_t> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);Require(bool(input),"SPIR-V read failed");
 for(unsigned trial=0;trial<2;++trial)for(unsigned order=0;order<2;++order)Run((trial+order)%2,trial,code);
 std::cout<<"PASS actual GPU timestamps AB/BA: compute+copy snapshots, reset NOT_READY, same-ticket completion availability,8-slot reuse rounds, exact guards; inclusive overlapping regions not frame GPU attribution\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
