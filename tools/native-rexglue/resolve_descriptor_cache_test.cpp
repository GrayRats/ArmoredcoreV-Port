#include "resolve_descriptor_cache.h"
#include <cassert>
#include <map>
#include <iostream>
using Cache=acv::ResolveDescriptorCache;
template<class T> T Handle(uintptr_t value) {return reinterpret_cast<T>(value);}
struct Fake {
 unsigned creates=0,allocations=0,updates=0,destroys=0;
 VkResult create_result=VK_SUCCESS,allocate_result=VK_SUCCESS;
 std::map<VkDescriptorSet,VkDescriptorBufferInfo> bindings;
 static Fake* active;
 static VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice,const VkDescriptorPoolCreateInfo* info,const VkAllocationCallbacks*,VkDescriptorPool* out) {
  auto& f=*active;++f.creates;
  assert(info->maxSets==64 && info->poolSizeCount==1 && info->flags==0);
  assert(info->pPoolSizes->type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && info->pPoolSizes->descriptorCount==64);
  *out=Handle<VkDescriptorPool>(0xBAD);return f.create_result;
 }
 static VKAPI_ATTR VkResult VKAPI_CALL Allocate(VkDevice,const VkDescriptorSetAllocateInfo* info,VkDescriptorSet* out) {
  auto& f=*active;++f.allocations;assert(info->descriptorSetCount==1 && info->descriptorPool);
  *out=Handle<VkDescriptorSet>(100+f.allocations);return f.allocate_result;
 }
 static VKAPI_ATTR void VKAPI_CALL Update(VkDevice,uint32_t count,const VkWriteDescriptorSet* write,uint32_t copies,const VkCopyDescriptorSet*) {
  auto& f=*active;++f.updates;assert(count==1 && !copies && write->dstBinding==0 && write->descriptorCount==1);
  assert(write->descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  // Updating an earlier set would invalidate an in-flight immutable binding.
  assert(!f.bindings.contains(write->dstSet));f.bindings[write->dstSet]=*write->pBufferInfo;
 }
 static VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice,VkDescriptorPool pool,const VkAllocationCallbacks*) {assert(pool);++active->destroys;}
 Cache::Api api(){active=this;return {Create,Allocate,Update,Destroy};}
};Fake* Fake::active=nullptr;
int main() {
 auto device=Handle<VkDevice>(1);auto layout=Handle<VkDescriptorSetLayout>(2);auto buffer=Handle<VkBuffer>(3);
 {
  Fake f;Cache c(f.api(),device,layout,buffer,1<<20,4096,256);c.EnableStatistics();
  for(auto key: {VkDescriptorBufferInfo{buffer,1,4},{buffer,0,0},{buffer,0,VK_WHOLE_SIZE},
       {buffer,0,8192},{buffer,1<<20,4},{buffer,(1<<20)-256,512},
       {buffer,UINT64_MAX-255,512},{Handle<VkBuffer>(4),0,4}}) assert(c.Get(key).status==Cache::Status::Invalid);
  assert(!f.creates);
  auto first=c.Get({buffer,256,128});assert(first.status==Cache::Status::Ready && !first.hit);
  auto same=c.Get({buffer,256,128});assert(same.hit && same.set==first.set && f.updates==1);
  auto range=c.Get({buffer,256,256});assert(range.set!=first.set);
  auto offset=c.Get({buffer,512,128});assert(offset.set!=first.set);
  assert(f.bindings.at(first.set).offset==256 && f.bindings.at(first.set).range==128);
  for(uint32_t i=3;i<64;++i) assert(c.Get({buffer,VkDeviceSize(i+2)*256,128}).status==Cache::Status::Ready);
  assert(c.size()==64 && f.creates==1 && f.allocations==64 && f.updates==64);
  assert(c.Get({buffer,65536,128}).status==Cache::Status::Full);
  assert(c.Get({buffer,256,128}).set==first.set && f.updates==64);
  assert(!c.DestroyAfterDeviceStopped(false) && f.destroys==0 && c.size()==64);
  assert(c.DestroyAfterDeviceStopped(true) && f.destroys==1);
  assert(c.Get({buffer,256,128}).status==Cache::Status::Invalid);
  assert(c.DestroyAfterDeviceStopped(true) && f.destroys==1);
 }
 for(VkResult failure:{VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_OUT_OF_DEVICE_MEMORY,VK_ERROR_OUT_OF_POOL_MEMORY,VK_ERROR_FRAGMENTED_POOL,VK_ERROR_DEVICE_LOST}) {
  for(bool create_failure:{false,true}) {
   Fake f;Cache c(f.api(),device,layout,buffer,1<<20,4096,256);
   if(create_failure) f.create_result=failure;else f.allocate_result=failure;
   auto result=c.Get({buffer,0,128});assert(result.status==Cache::Status::ApiFailure && result.vk_result==failure && c.size()==0 && f.updates==0);
   unsigned calls=f.creates+f.allocations;
   result=c.Get({buffer,256,128});
   assert(result.status==(Cache::ResourceExhaustion(failure)?Cache::Status::Full:Cache::Status::ApiFailure));
   assert(calls==f.creates+f.allocations && !f.updates);
   assert(c.DestroyAfterDeviceStopped(true));assert(f.destroys==(create_failure?0:1));
  }
 }
 {
  Fake f;Cache c(f.api(),device,layout,buffer,1<<20,4096,256);
  auto first=c.Get({buffer,256,128});f.allocate_result=VK_ERROR_OUT_OF_POOL_MEMORY;
  assert(c.Get({buffer,512,128}).status==Cache::Status::ApiFailure);
  assert(c.Get({buffer,256,128}).set==first.set && f.updates==1 && c.size()==1);
  assert(c.DestroyAfterDeviceStopped(true));
  // The recycled buffer handle belongs to a NEW context/cache lifetime.
  Cache next(f.api(),device,layout,buffer,1<<20,4096,256);f.allocate_result=VK_SUCCESS;
  auto fresh=next.Get({buffer,256,128});assert(!fresh.hit && fresh.set!=first.set);
  assert(next.DestroyAfterDeviceStopped(true));
 }
 std::cout<<"PASS immutable descriptors: exact keys, limits/alignment/overflow,64cap/fullfallback,inflightimmutability,resource/device-loss failures,retained pool until stop,new buffer lifetime\n";
}
