#include "buffer_bind_cache.h"
#include <cassert>
#include <iostream>
int main(){
 auto b=reinterpret_cast<VkBuffer>(uintptr_t(1));auto c=reinterpret_cast<VkBuffer>(uintptr_t(2));
 acv::BufferBindCache cache;assert(cache.Index(b,0,VK_INDEX_TYPE_UINT16));assert(!cache.Index(b,0,VK_INDEX_TYPE_UINT16));assert(cache.Index(b,2,VK_INDEX_TYPE_UINT16));assert(cache.Index(b,2,VK_INDEX_TYPE_UINT32));
 VkBuffer buffers[]={b,c};VkDeviceSize offsets[]={0,16};assert(cache.Vertices(0,2,buffers,offsets));assert(!cache.Vertices(0,2,buffers,offsets));
 assert(cache.Vertices(1,1,buffers,offsets));assert(cache.Vertices(0,2,buffers,offsets));assert(cache.Vertices(32,1,buffers,offsets));assert(cache.Vertices(0,2,buffers,offsets));
 acv::BufferBindCache fresh;assert(fresh.Index(b,2,VK_INDEX_TYPE_UINT32));
 acv::PushConstantCache push;uint32_t data[]={1,2};auto layout=reinterpret_cast<VkPipelineLayout>(uintptr_t(1));
 assert(push.Update(layout,1,0,8,data));assert(!push.Update(layout,1,0,8,data));data[1]=3;assert(push.Update(layout,1,0,8,data));assert(push.Update(layout,2,0,8,data));assert(push.Update(layout,2,4,8,data));assert(push.Update(layout,2,0,8,data));
 auto other=reinterpret_cast<VkPipelineLayout>(uintptr_t(2));assert(push.Update(other,2,0,8,data));assert(push.Update(layout,2,0,8,data));
 uint8_t large[260]{};assert(push.Update(layout,2,0,260,large));assert(push.Update(layout,2,0,8,data));
 acv::DynamicConstantCache<4> blend;float values[]={0,1,2,3};assert(blend.Update(values));assert(!blend.Update(values));values[2]=4;assert(blend.Update(values));blend.Reset();assert(blend.Update(values));
 acv::DynamicConstantCache<3> depth;assert(depth.Update(values));assert(!depth.Update(values));depth.Reset();assert(depth.Update(values));
 acv::PushConstantCache stencil;uint32_t mask=0xFF;
 assert(stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_FRONT_BIT,0,4,&mask));assert(!stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_FRONT_BIT,0,4,&mask));
 assert(stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_BACK_BIT,0,4,&mask));assert(stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_FRONT_AND_BACK,0,4,&mask));
 mask=0;assert(stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_FRONT_AND_BACK,0,4,&mask));stencil={};assert(stencil.Update(VK_NULL_HANDLE,VK_STENCIL_FACE_FRONT_AND_BACK,0,4,&mask));
 std::cout<<"PASS exact index/vertex/push dedup, offsets/types/stages/layout/overlap/fallback/new command buffer\n";
}
