#pragma once
#include <vulkan/vulkan_core.h>
#include <array>
#include <cstdint>
namespace acv {
// Only ordinary descriptor binds mutate this cache. Any future push-descriptor
// or secondary-command opcode must invalidate it. One instance per Execute.
class DescriptorBindCache {
 std::array<VkDescriptorSet,16> sets_{};
 std::array<uint32_t,32> offsets_{};
 VkPipelineBindPoint point_{};VkPipelineLayout layout_{};
 uint32_t first_=0,count_=0,dynamic_count_=0;bool valid_=false;
 public:
 void Invalidate(){valid_=false;}
 bool Update(VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,
             uint32_t count,const VkDescriptorSet* sets,uint32_t dynamic_count,const uint32_t* offsets) {
  if(count>sets_.size() || dynamic_count>offsets_.size()){Invalidate();return true;}
  bool same=valid_ && point_==point && layout_==layout && first_==first && count_==count && dynamic_count_==dynamic_count;
  for(uint32_t i=0;i<count;++i)same &= sets_[i]==sets[i];
  for(uint32_t i=0;i<dynamic_count;++i)same &= offsets_[i]==offsets[i];
  if(same)return false;
  for(uint32_t i=0;i<count;++i)sets_[i]=sets[i];
  for(uint32_t i=0;i<dynamic_count;++i)offsets_[i]=offsets[i];
  point_=point;layout_=layout;first_=first;count_=count;dynamic_count_=dynamic_count;valid_=true;return true;
 }
};
}
