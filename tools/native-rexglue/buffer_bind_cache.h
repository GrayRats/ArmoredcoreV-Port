#pragma once
#include <vulkan/vulkan_core.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace acv {
// Command-buffer-local state; only exact redundant buffer bindings are omitted.
class BufferBindCache {
 struct Vertex { VkBuffer buffer{}; VkDeviceSize offset{}; bool valid=false; };
 std::array<Vertex,32> vertices_{};
 VkBuffer index_{}; VkDeviceSize offset_{}; VkIndexType type_{}; bool valid_=false;
 public:
 bool Index(VkBuffer buffer,VkDeviceSize offset,VkIndexType type) {
  if(valid_ && index_==buffer && offset_==offset && type_==type)return false;
  index_=buffer;offset_=offset;type_=type;valid_=true;return true;
 }
 bool Vertices(uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets) {
  if(first>vertices_.size() || count>vertices_.size()-first){vertices_={};return true;}
  bool changed=false;
  for(uint32_t i=0;i<count;++i){auto& v=vertices_[first+i];changed|=!v.valid || v.buffer!=buffers[i] || v.offset!=offsets[i];v={buffers[i],offsets[i],true};}
  return changed;
 }
};
class PushConstantCache {
 std::array<uint8_t,256> bytes_{}; VkPipelineLayout layout_{};
 VkShaderStageFlags stages_{}; uint32_t offset_=0,size_=0;bool valid_=false;
 public:
 bool Update(VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void* data) {
  if(size>bytes_.size()){valid_=false;return true;}
  if(valid_ && layout_==layout && stages_==stages && offset_==offset && size_==size && !std::memcmp(bytes_.data(),data,size))return false;
  std::memcpy(bytes_.data(),data,size);layout_=layout;stages_=stages;offset_=offset;size_=size;valid_=true;return true;
 }
};
template<size_t N> class DynamicConstantCache {
 std::array<float,N> values_{};bool valid_=false;
 public:
 void Reset(){valid_=false;}
 bool Update(const float* values){if(valid_ && !std::memcmp(values_.data(),values,sizeof(float)*N))return false;std::memcpy(values_.data(),values,sizeof(float)*N);valid_=true;return true;}
};

}
