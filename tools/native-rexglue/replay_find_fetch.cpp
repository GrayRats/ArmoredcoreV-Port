// Locate draws with a physical texture fetch, using captured shader constants.
// Reports declared shader use separately: a populated fetch slot may be unused.
#include "renderdoc_replay.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
REPLAY_PROGRAM_MARKER();
struct Lifetime { Lifetime(){RENDERDOC_InitialiseReplay(GlobalEnvironment{},{});} ~Lifetime(){RENDERDOC_ShutdownReplay();} };
struct CaptureClose {void operator()(ICaptureFile* p)const{if(p)p->Shutdown();}};
struct ReplayClose {void operator()(IReplayController* p)const{if(p)p->Shutdown();}};
static void Walk(const rdcarray<ActionDescription>& in,std::vector<const ActionDescription*>& out){
 for(const auto& a:in){out.push_back(&a);Walk(a.children,out);}
}
static bool Number(const char* s,uint32_t& value){
 errno=0;char* end=nullptr;auto n=std::strtoull(s,&end,0);
 if(errno||end==s||*end||*s=='-'||n>UINT32_MAX)return false;value=uint32_t(n);return true;
}
int main(int argc,char** argv){
 uint32_t physical,elements=0;
 if((argc!=3&&argc!=4)||!Number(argv[2],physical)||(argc==4&&!Number(argv[3],elements))){
  std::cerr<<"Usage: replay_find_fetch CAPTURE.rdc PHYSICAL_ADDRESS [ELEMENTS]\n";return 2;
 }
 Lifetime lifetime;
 std::unique_ptr<ICaptureFile,CaptureClose> cap(RENDERDOC_OpenCaptureFile());
 if(!cap)return 1;
 auto opened=cap->OpenFile(argv[1],"rdc",nullptr);if(!opened.OK())return 1;
 auto result=cap->OpenCapture(ReplayOptions{},nullptr);
 if(!result.first.OK()||!result.second)return 1;
 std::unique_ptr<IReplayController,ReplayClose> replay(result.second);
 std::vector<const ActionDescription*> actions;Walk(replay->GetRootActions(),actions);
 uint32_t scanned=0,matches=0;bool first=true;
 std::cout<<"{\"matches\":[";
 for(auto a:actions){
  if((a->flags&ActionFlags::Drawcall)==ActionFlags::NoFlags)continue;
  if(elements&&a->numIndices!=elements)continue;
  replay->SetFrameEvent(a->eventId,true);++scanned;
  auto vk=replay->GetVulkanPipelineState();
  if(!vk||!vk->fragmentShader.reflection)continue;
  auto refl=vk->fragmentShader.reflection;
  for(const auto& access:replay->GetDescriptorAccess()){
   if(access.stage!=ShaderStage::Pixel||access.type!=DescriptorType::ConstantBuffer||access.index>=refl->constantBlocks.size())continue;
   const auto& block=refl->constantBlocks[access.index];
   // SDK uniform group1/binding4 is the 32x6-dword fetch-constant buffer.
   if(block.fixedBindSetOrSpace!=1||block.fixedBindNumber!=4||block.byteSize<768)continue;
   auto desc=replay->GetDescriptors(access.descriptorStore,{DescriptorRange(access)});if(desc.empty())continue;
   auto data=replay->GetBufferData(desc[0].resource,desc[0].byteOffset,768);if(data.size()!=768)return 1;
   for(uint32_t slot=0;slot<32;++slot){
    uint32_t w[6];std::memcpy(w,data.data()+slot*24,sizeof(w));
    if((w[0]&3)!=2||(w[1]&0xfffff000u)!=physical)continue;
    bool used=false;std::string prefix="xe_texture"+std::to_string(slot)+"_";
    for(const auto& r:refl->readOnlyResources)if(std::string(r.name.c_str()).find(prefix)==0)used=true;
    std::cout<<(first?"":",")<<"{\"event\":"<<a->eventId<<",\"elements\":"<<a->numIndices
     <<",\"slot\":"<<slot<<",\"shader_declares_slot\":"<<(used?"true":"false")
     <<",\"format\":"<<(w[1]&63)<<",\"width\":"<<((w[2]&8191)+1)
     <<",\"height\":"<<(((w[2]>>13)&8191)+1)<<",\"words\":[";
    for(unsigned j=0;j<6;++j)std::cout<<(j?",":"")<<w[j];
    std::cout<<"]}";first=false;++matches;
   }
  }
 }
 std::cout<<"],\"scanned_draws\":"<<scanned<<",\"match_count\":"<<matches<<"}\n";
 return 0;
}
