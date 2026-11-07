#pragma once
// Include after the embedding SDK's Vulkan API/platform configuration.
#include <vulkan/vulkan_core.h>
#include <array>
#include <cstdint>
#include <exception>
namespace acv {
// Immutable storage-buffer binding0 descriptors for ONE stable buffer lifetime.
// The owner must outlive pending command buffers and explicitly stop the GPU
// before destroying this pool. A new buffer lifetime requires a new cache even
// if Vulkan recycles the same handle. No in-flight eviction/reset/update exists.
class ResolveDescriptorCache {
 public:
  static constexpr uint32_t kCapacity=64;
  struct Api {
    PFN_vkCreateDescriptorPool create_pool;
    PFN_vkAllocateDescriptorSets allocate_sets;
    PFN_vkUpdateDescriptorSets update_sets;
    PFN_vkDestroyDescriptorPool destroy_pool;
  };
  enum class Status { Ready, Full, Invalid, ApiFailure };
  struct Result {
    Status status;VkDescriptorSet set=VK_NULL_HANDLE;
    VkResult vk_result=VK_SUCCESS;bool hit=false;
  };
  struct Statistics {uint64_t lookups=0,hits=0,updates=0,allocations=0,fallbacks=0,failures=0;};
  ResolveDescriptorCache(Api api,VkDevice device,VkDescriptorSetLayout layout,
      VkBuffer buffer,VkDeviceSize buffer_bytes,VkDeviceSize max_range,VkDeviceSize alignment)
      :api_(api),device_(device),layout_(layout),buffer_(buffer),buffer_bytes_(buffer_bytes),
       max_range_(max_range),alignment_(alignment) {}
  ~ResolveDescriptorCache() {if(pool_) std::terminate();}
  ResolveDescriptorCache(const ResolveDescriptorCache&)=delete;
  ResolveDescriptorCache& operator=(const ResolveDescriptorCache&)=delete;
  static bool ResourceExhaustion(VkResult result) {
    return result==VK_ERROR_OUT_OF_HOST_MEMORY || result==VK_ERROR_OUT_OF_DEVICE_MEMORY ||
        result==VK_ERROR_OUT_OF_POOL_MEMORY || result==VK_ERROR_FRAGMENTED_POOL;
  }
  Result Get(const VkDescriptorBufferInfo& key) {
    if(statistics_enabled_) ++statistics_.lookups;
    if(stopped_ || !device_ || !layout_ || !buffer_ || key.buffer!=buffer_ ||
       !alignment_ || key.offset%alignment_ || !key.range || key.range==VK_WHOLE_SIZE ||
       key.range>max_range_ || key.offset>=buffer_bytes_ || key.range>buffer_bytes_-key.offset)
      return {Status::Invalid};
    if(terminal_result_!=VK_SUCCESS) return {Status::ApiFailure,VK_NULL_HANDLE,terminal_result_};
    for(uint32_t i=0;i<count_;++i) {
      const auto& entry=entries_[i];
      if(entry.offset==key.offset && entry.range==key.range) {
        if(statistics_enabled_) ++statistics_.hits;
        return {Status::Ready,entry.set,VK_SUCCESS,true};
      }
    }
    if(count_==kCapacity || allocation_disabled_) {
      if(statistics_enabled_) ++statistics_.fallbacks;
      return {Status::Full};
    }
    if(!pool_) {
      VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,kCapacity};
      VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
      info.maxSets=kCapacity;info.poolSizeCount=1;info.pPoolSizes=&size;
      VkDescriptorPool created=VK_NULL_HANDLE;
      const VkResult result=api_.create_pool(device_,&info,nullptr,&created);
      if(result!=VK_SUCCESS) return Failed(result);
      pool_=created;
    }
    VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    info.descriptorPool=pool_;info.descriptorSetCount=1;info.pSetLayouts=&layout_;
    VkDescriptorSet set=VK_NULL_HANDLE;
    if(statistics_enabled_) ++statistics_.allocations;
    const VkResult result=api_.allocate_sets(device_,&info,&set);
    if(result!=VK_SUCCESS) return Failed(result);
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet=set;write.dstBinding=0;write.descriptorCount=1;
    write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&key;
    api_.update_sets(device_,1,&write,0,nullptr);
    if(statistics_enabled_) ++statistics_.updates;
    entries_[count_++]={key.offset,key.range,set};
    return {Status::Ready,set};
  }
  // The boolean is an explicit owner proof, not a fence query. No Vulkan calls
  // or releases on false. Production calls only after queue completion/loss.
  bool DestroyAfterDeviceStopped(bool stopped) {
    if(!stopped) return false;
    if(pool_) api_.destroy_pool(device_,pool_,nullptr);
    pool_=VK_NULL_HANDLE;count_=0;stopped_=true;return true;
  }
  uint32_t size() const {return count_;}
  void EnableStatistics() {statistics_enabled_=true;}
  const Statistics& statistics() const {return statistics_;}
 private:
  Result Failed(VkResult result) {
    if(statistics_enabled_) ++statistics_.failures;
    if(ResourceExhaustion(result)) allocation_disabled_=true;
    else terminal_result_=result;
    return {Status::ApiFailure,VK_NULL_HANDLE,result};
  }
  struct Entry {VkDeviceSize offset{},range{};VkDescriptorSet set{};};
  Api api_;VkDevice device_;VkDescriptorSetLayout layout_;VkBuffer buffer_;
  VkDeviceSize buffer_bytes_,max_range_,alignment_;VkDescriptorPool pool_=VK_NULL_HANDLE;
  std::array<Entry,kCapacity> entries_{};uint32_t count_=0;
  bool stopped_=false,allocation_disabled_=false,statistics_enabled_=false;
  VkResult terminal_result_=VK_SUCCESS;Statistics statistics_;
};
} // namespace acv
