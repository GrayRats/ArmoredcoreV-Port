#include "descriptor_bind_cache.h"
#include <cassert>
#include <iostream>
int main(){
 acv::DescriptorBindCache c;auto layout=reinterpret_cast<VkPipelineLayout>(uintptr_t(1));auto other=reinterpret_cast<VkPipelineLayout>(uintptr_t(2));
 VkDescriptorSet sets[]={reinterpret_cast<VkDescriptorSet>(uintptr_t(1)),reinterpret_cast<VkDescriptorSet>(uintptr_t(2))};uint32_t offsets[]={16,32};
 auto bind=[&](VkPipelineBindPoint point,VkPipelineLayout l,uint32_t first){return c.Update(point,l,first,2,sets,2,offsets);};
 assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0));for(int i=0;i<10000;++i)assert(!bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0));
 offsets[1]=64;assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0));assert(bind(VK_PIPELINE_BIND_POINT_COMPUTE,layout,0));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,other,0));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));
 sets[1]=sets[0];assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));
 assert(c.Update(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,1,sets,0,nullptr));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));
 assert(c.Update(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,17,nullptr,0,nullptr));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));
 assert(c.Update(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,0,nullptr,33,nullptr));assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));
 c.Invalidate();assert(bind(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1));acv::DescriptorBindCache fresh;assert(fresh.Update(VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,2,sets,2,offsets));
 std::cout<<"PASS10000duplicates; offsets/layout/bindpoint/firstSet/count/set changes, overlap disturbance, bounded fallback and reset\n";
}
