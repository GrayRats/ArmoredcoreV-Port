#include "gpu_timestamp_profiler.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef NDEBUG
#error Assertions required
#endif
using P=acv::GpuTimestampProfiler;
namespace {
unsigned creates=0,gets=0,destroys=0;VkResult create_result=VK_SUCCESS,get_result=VK_SUCCESS;bool available=true;
VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice,const VkQueryPoolCreateInfo* ci,const VkAllocationCallbacks*,VkQueryPool* pool){
 ++creates;assert(ci->queryType==VK_QUERY_TYPE_TIMESTAMP && ci->queryCount==2048);*pool=reinterpret_cast<VkQueryPool>(uintptr_t(1));return create_result;
}
VKAPI_ATTR VkResult VKAPI_CALL Get(VkDevice,VkQueryPool,uint32_t first,uint32_t count,size_t bytes,void* data,VkDeviceSize stride,VkQueryResultFlags flags){
 ++gets;assert(first%32==0 && count<=32 && count%2==0 && bytes==count*sizeof(P::Value));
 assert(stride==sizeof(P::Value) && flags==(VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
 auto* values=static_cast<P::Value*>(data);for(unsigned i=0;i<count;++i)values[i]={100+10*i,uint64_t(available)};return get_result;
}
VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice,VkQueryPool,const VkAllocationCallbacks*){++destroys;}
P::Api Api(){return{Create,Get,Destroy};}
struct Commands {
 std::vector<uint32_t> resets,writes;std::vector<VkPipelineStageFlagBits> stages;
 void Reset(VkQueryPool,uint32_t first,uint32_t count){assert(count==32 && first%32==0);resets.push_back(first);}
 void Write(VkPipelineStageFlagBits stage,VkQueryPool,uint32_t query){writes.push_back(query);stages.push_back(stage);}
 auto ResetFn(){return[this](auto...v){Reset(v...);};}auto WriteFn(){return[this](auto...v){Write(v...);};}
};
}
int main(){
 {
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();
 assert(p.Begin(1,120,reset));p.Mark(1,3,write,4096);p.Mark(1,3,write,8192);
 p.Mark(1,1,write);p.Seal(1,write);p.Submitted(1);
 p.Collect(1,120.1,[](const auto&r,const auto&){assert(r.spans[0].commands==2 && r.spans[0].copy_bytes==12288);assert(r.spans[1].copy_bytes==0);});
 assert(p.Begin(2,120.2,reset));p.Mark(2,3,write,256);p.Seal(2,write);p.Submitted(2);
 p.Collect(2,120.3,[](const auto&r,const auto&){assert(r.spans[0].copy_bytes==256);});
 p.DestroyAfterDeviceStopped(true);
 }
 assert(P::Delta(250,3,8)==9 && P::Delta(UINT64_MAX-2,3,64)==6 && P::Delta(0x1FA,0x203,8)==9 && P::Delta(1,2,0)==0);
 {auto api=Api();api.get=nullptr;unsigned before=creates;P p(api,{},64,1);assert(!p.enabled()&&p.last_result()==VK_ERROR_INITIALIZATION_FAILED&&creates==before);}
 {unsigned before=destroys;{P p(Api(),{},64,1);}assert(destroys==before+1);} // constructor-owner unwind, no recorded commands
 for(auto bits:{0u,65u}){unsigned before=creates;P p(Api(),{},bits,1);assert(!p.enabled()&&creates==before);p.DestroyAfterDeviceStopped(true);}
 for(double period:{0.,-1.,std::numeric_limits<double>::infinity()}){P p(Api(),{},64,period);assert(!p.enabled());p.DestroyAfterDeviceStopped(true);}
 for(auto error:{VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_DEVICE_LOST}){create_result=error;P p(Api(),{},64,1);assert(!p.enabled()&&p.last_result()==error && p.pool()==VK_NULL_HANDLE);unsigned before=destroys;p.DestroyAfterDeviceStopped(true);assert(destroys==before);}create_result=VK_SUCCESS;
 {
 P p(Api(),{},64,1);Commands cmd;auto reset=cmd.ResetFn();auto write=cmd.WriteFn();
 assert(!p.Begin(1,119.99,reset)&&!p.Begin(1,180,reset));assert(p.Begin(1,120,reset));
 for(unsigned i=0;i<100000;++i)p.Mark(1,1,write); // Coalesced, not per-draw pairs.
 p.Mark(1,2,write);p.Mark(1,3,write);p.Seal(1,write);p.Submitted(1);
 assert(cmd.writes==std::vector<uint32_t>({0,1,2,3,4,5}));
 for(unsigned i=0;i<cmd.stages.size();++i)assert(cmd.stages[i]==(i%2?VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
 unsigned before=gets,emitted=0;p.Collect(0,120.1,[&](auto&,auto&){assert(false);});assert(gets==before);
 p.Collect(1,120.1,[&](const auto& r,const auto& ns){++emitted;assert(r.spans[0].commands==100000 && r.regions==3);assert(ns[0]==10&&ns[1]==10&&ns[2]==10);});
 assert(emitted==1&&!p.occupied());assert(!p.Begin(2,120.01,reset));assert(p.Begin(2,120.1,reset));
 for(unsigned i=0;i<40;++i)p.Mark(2,1+i%3,write);p.Seal(2,write);p.Submitted(2);
 assert(cmd.writes.size()==6+32&&p.stats().truncated==1);
 p.Collect(2,120.2,[&](const auto&r,const auto&){assert(r.truncated && r.regions==16);});
 assert(!p.DestroyAfterDeviceStopped(false));assert(p.DestroyAfterDeviceStopped(true));
 }
 for(bool availability_false:{false,true}){
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn(),unused=c.ResetFn();auto write=c.WriteFn();(void)unused;
 p.Begin(1,120,reset);p.Mark(1,1,write);p.Seal(1,write);p.Submitted(1);
 get_result=availability_false?VK_SUCCESS:VK_NOT_READY;available=!availability_false;
 unsigned before=gets;for(unsigned i=0;i<3;++i)p.Collect(0,120.2,[](auto&,auto&){assert(false);});assert(gets==before);
 for(unsigned i=0;i<3;++i){p.Collect(1,120.2,[](auto&,auto&){assert(false);});assert(p.occupied()==(i<2));}
 assert(p.stats().not_ready==3 && p.stats().discarded==1);get_result=VK_SUCCESS;available=true;p.DestroyAfterDeviceStopped(true);
 }
 for(auto error:{VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_DEVICE_LOST,VK_ERROR_UNKNOWN}){
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();p.Begin(1,120,reset);p.Mark(1,1,write);p.Seal(1,write);p.Submitted(1);
 get_result=error;assert(p.Collect(1,120.2,[](auto&,auto&){assert(false);})==error);assert(!p.enabled()&&p.occupied()==1);unsigned before=gets;p.Collect(1,120.3,[](auto&,auto&){});assert(gets==before);get_result=VK_SUCCESS;p.DestroyAfterDeviceStopped(true);
 }
 {
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();
 for(unsigned i=0;i<64;++i){assert(p.Begin(i+1,120+0.1*i,reset));p.Mark(i+1,1,write);p.Seal(i+1,write);p.Submitted(i+1);}
 assert(p.occupied()==64&&!p.Begin(65,127,reset)&&c.resets.size()==64);
 p.Collect(1,127.1,[](auto&,auto&){});assert(p.occupied()==63);assert(p.Begin(65,127.2,reset)&&c.resets.back()==0);
 p.Mark(999,1,write);assert(p.stats().failed==1); // wrong ticket retains range.
 p.DestroyAfterDeviceStopped(true);
 }
 {
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();
 p.Begin(1,120,reset);p.Mark(1,1,write);
 {acv::GpuTimestampSubmissionScope scope(&p,1);p.Seal(1,write);} // injected EndSubmission failure
 p.Collect(999,130,[](auto&,auto&){assert(false);});assert(p.occupied()==1&&p.stats().failed==1);
 p.Begin(2,120.1,reset);p.Mark(2,2,write);{acv::GpuTimestampSubmissionScope scope(&p,2);p.Seal(2,write);scope.Submitted();}
 p.Collect(2,120.2,[](const auto&r,auto&){assert(r.ticket==2);});assert(p.occupied()==1);
 p.DestroyAfterDeviceStopped(true);
 }
 {P p(Api(),{},8,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();p.Begin(1,120,reset);p.Mark(1,1,write);p.Seal(1,write);p.Submitted(1);p.Collect(1,120.1,[](auto&,auto&){assert(false);});assert(p.stats().ambiguous==1);p.DestroyAfterDeviceStopped(true);}
 {P p(Api(),{},64,1);p.Begin(1,120,[](auto...){throw std::runtime_error("injected deferred write failure");});assert(!p.enabled()&&p.occupied()==1&&p.stats().failed==1);p.DestroyAfterDeviceStopped(true);}
 {
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();unsigned before=gets;
 assert(!p.Begin(1,std::numeric_limits<double>::quiet_NaN(),reset));
 for(unsigned i=0;i<1200;++i){double age=120+i*0.050001;assert(p.Begin(i+1,age,reset));p.Seal(i+1,write);p.Submitted(i+1);p.Collect(i+1,age+0.001,[](auto&,auto&){assert(false);});}
 assert(p.stats().selected==1200 && p.stats().discarded==1200 && gets==before && !p.Begin(1201,180,reset));p.DestroyAfterDeviceStopped(true);
 }
 for(bool throw_end:{false,true}){
 P p(Api(),{},64,1);Commands c;auto reset=c.ResetFn();auto write=c.WriteFn();p.Begin(1,120,reset);
 auto bad=[](auto...){throw std::runtime_error("injected timestamp append failure");};
 if(throw_end){p.Mark(1,1,write);p.Seal(1,bad);}else p.Mark(1,1,bad);
 assert(!p.enabled() && p.occupied()==1 && p.stats().failed==1);assert(!p.Begin(2,121,reset));p.DestroyAfterDeviceStopped(true);
 }
 std::cout<<"PASS shared GPU timestamp ring: coalescing100k draws, exact paired capacity,64-slot bound, ticket/completion/reset guards, rate/window, native NOT_READY/availability/failed API, submission failure retention, wrap ambiguity, teardown\n";
}
