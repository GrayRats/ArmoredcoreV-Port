// Host diagnostic: RenderDoc 1.46 public replay API, no image export or guest patch.
// Header provenance: logs/renderdoc-replay-headers.json. Link to matching librenderdoc.
// Linux build requires -DRENDERDOC_PLATFORM_LINUX -rdynamic -lrenderdoc:
// RenderDoc must find renderdoc__replay__marker in the dynamic symbol table.
#include "renderdoc_replay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <vector>
REPLAY_PROGRAM_MARKER();

struct ReplayLifetime {
  ReplayLifetime() { RENDERDOC_InitialiseReplay(GlobalEnvironment{}, {}); }
  ~ReplayLifetime() { RENDERDOC_ShutdownReplay(); }
};
struct CloseCapture { void operator()(ICaptureFile* p) const { if(p) p->Shutdown(); } };
struct CloseReplay { void operator()(IReplayController* p) const { if(p) p->Shutdown(); } };

void Walk(const rdcarray<ActionDescription>& actions, std::vector<const ActionDescription*>& flat) {
  for(const auto& a : actions) { flat.push_back(&a); Walk(a.children, flat); }
}
void Values(const PixelValue& p,CompType type) {
  std::cout << '[';
  for(int c=0;c<4;++c) {
    if(c) std::cout << ',';
    if(type==CompType::UInt) std::cout << p.uintValue[c];
    else if(type==CompType::SInt) std::cout << p.intValue[c];
    else if(std::isfinite(p.floatValue[c])) std::cout << p.floatValue[c];
    else std::cout << "null";
  }
  std::cout << ']';
}
int main(int argc, char** argv) {
  const bool inspect_mesh=argc==4 && std::strcmp(argv[3],"--mesh")==0;
  if(argc<2 || argc>4 || (argc==4 && !inspect_mesh)) {
    std::cerr << "Usage: replay_inspect CAPTURE.rdc [EVENT_ID [--mesh]]\n"; return 2;
  }
  ReplayLifetime lifetime;
  std::unique_ptr<ICaptureFile,CloseCapture> capture(RENDERDOC_OpenCaptureFile());
  if(!capture) { std::cerr << "OpenCaptureFile returned null\n"; return 1; }
  auto opened=capture->OpenFile(argv[1], "rdc", nullptr);
  if(!opened.OK()) { std::cerr << static_cast<unsigned>(opened.code) << '\n'; return 1; }
  auto replayed=capture->OpenCapture(ReplayOptions{},nullptr);
  if(!replayed.first.OK() || !replayed.second) {
    std::cerr << static_cast<unsigned>(replayed.first.code) << '\n'; return 1;
  }
  std::unique_ptr<IReplayController,CloseReplay> replay(replayed.second);
  std::vector<const ActionDescription*> actions;
  Walk(replay->GetRootActions(),actions);
  uint32_t event=0, draws=0, dispatches=0;
  for(auto a:actions) {
    if((a->flags & ActionFlags::Drawcall)!=ActionFlags::NoFlags) { ++draws; event=a->eventId; }
    if((a->flags & ActionFlags::Dispatch)!=ActionFlags::NoFlags) ++dispatches;
  }
  if(argc>=3) {
    char* end=nullptr; const auto parsed=std::strtoul(argv[2],&end,10);
    if(end==argv[2] || *end!='\0' || parsed>UINT32_MAX) return 2;
    event=static_cast<uint32_t>(parsed);
    bool found=false; for(auto a:actions) if(a->eventId==event) found=true;
    if(!found) { std::cerr << "Requested event is not an action\n"; return 2; }
  }
  if(!event) { std::cerr << "No draw event in capture\n"; return 1; }
  replay->SetFrameEvent(event,true);
  const ActionDescription* selected=nullptr;
  for(auto a:actions) if(a->eventId==event) selected=a;
  std::cout << "{\"event\":" << event << ",\"draw_actions\":" << draws
            << ",\"dispatch_actions\":" << dispatches << ",\"tail_draw_events\":[";
  size_t start=actions.size()>64?actions.size()-64:0; bool first=true;
  for(size_t i=start;i<actions.size();++i) {
    auto a=actions[i];
    if((a->flags & ActionFlags::Drawcall)==ActionFlags::NoFlags) continue;
    if(!first) std::cout << ','; first=false;
    std::cout << "{\"event\":" << a->eventId << ",\"elements\":" << a->numIndices << '}';
  }
  std::cout << "],\"dispatch_events\":[";first=true;
  for(auto a:actions) {
    if((a->flags & ActionFlags::Dispatch)==ActionFlags::NoFlags) continue;
    if(!first) std::cout << ',';first=false;
    std::cout << a->eventId;
  }
  std::cout << "],\"textures\":[";
  const auto& textures=replay->GetTextures(); std::set<ResourceId> seen; bool first_tex=true;
  std::vector<std::pair<Descriptor,const char*>> buffer_descriptors;
  auto describe=[&](const Descriptor& descriptor,const char* role) {
    if(descriptor.resource==ResourceId::Null() || !seen.insert(descriptor.resource).second) return;
    const TextureDescription* tex=nullptr; size_t index=0;
    for(size_t i=0;i<textures.size();++i) if(textures[i].resourceId==descriptor.resource) {tex=&textures[i];index=i;break;}
    if(!tex) { buffer_descriptors.emplace_back(descriptor,role);return; }
    if(tex->depth!=1 || tex->msSamp>1 || descriptor.firstMip>=tex->mips ||
       descriptor.firstSlice>=tex->arraysize) return;
    const Subresource sub(descriptor.firstMip,descriptor.firstSlice,0);
    auto range=replay->GetMinMax(tex->resourceId,sub,CompType::Typeless);
    if(!first_tex) std::cout << ',';first_tex=false;
    std::cout << "{\"role\":\"" << role << "\",\"texture_index\":" << index
              << ",\"width\":" << tex->width << ",\"height\":" << tex->height
              << ",\"component_bytes\":" << unsigned(tex->format.compByteWidth)
              << ",\"component_type\":" << unsigned(tex->format.compType)
              << ",\"view_component_type\":" << unsigned(descriptor.format.compType)
              << ",\"view_srgb\":" << (descriptor.format.SRGBCorrected()?"true":"false")
              << ",\"resource_bgra\":" << (tex->format.BGRAOrder()?"true":"false")
              << ",\"descriptor_type\":" << unsigned(descriptor.type)
              << ",\"min\":";Values(range.first,tex->format.compType);
    std::cout << ",\"max\":";Values(range.second,tex->format.compType);
    const auto width=std::max(1U,tex->width >> std::min(unsigned(descriptor.firstMip),31U));
    const auto height=std::max(1U,tex->height >> std::min(unsigned(descriptor.firstMip),31U));
    auto center=replay->PickPixel(tex->resourceId,width/2,height/2,sub,CompType::Typeless);
    std::cout << ",\"center\":";Values(center,tex->format.compType);
    if(tex->format.compType==CompType::UNorm || tex->format.compType==CompType::Float) {
      // Values outside [0,1] are excluded by the replay API, not clamped.
      std::cout << ",\"histogram_rgb_0_to_1\":[";
      for(int channel=0;channel<3;++channel) {
        if(channel) std::cout << ',';
        rdcfixedarray<bool,4> channels={false,false,false,false};channels[channel]=true;
        auto histogram=replay->GetHistogram(tex->resourceId,sub,CompType::Typeless,0.0f,1.0f,channels);
        std::cout << '[';
        for(size_t bucket=0;bucket<histogram.size();++bucket) {
          if(bucket) std::cout << ',';std::cout << histogram[bucket];
        }
        std::cout << ']';
      }
      std::cout << ']';
    }
    std::cout << ",\"usage\":[";bool first_usage=true;
    for(const auto& usage:replay->GetUsage(tex->resourceId)) {
      if(!first_usage) std::cout << ','; first_usage=false;
      std::cout << "{\"event\":" << usage.eventId << ",\"kind\":" << unsigned(usage.usage) << '}';
    }
    std::cout << "]}";
  };
  if(selected) for(auto id:selected->outputs) { Descriptor d;d.resource=id;describe(d,"output"); }
  const bool selected_dispatch=selected &&
      (selected->flags & ActionFlags::Dispatch)!=ActionFlags::NoFlags;
  for(const auto& access:replay->GetDescriptorAccess()) {
    if(access.stage!=(selected_dispatch?ShaderStage::Compute:ShaderStage::Pixel) ||
       access.descriptorStore==ResourceId::Null()) continue;
    rdcarray<DescriptorRange> ranges; ranges.push_back(DescriptorRange(access));
    for(const auto& d:replay->GetDescriptors(access.descriptorStore,ranges))
      describe(d,access.stage==ShaderStage::Pixel?"pixel_descriptor":"compute_descriptor");
  }
  std::cout << "],\"buffers\":["; bool first_buffer=true;
  const auto& buffers=replay->GetBuffers();
  for(const auto& item:buffer_descriptors) {
    for(size_t i=0;i<buffers.size();++i) {
      if(buffers[i].resourceId!=item.first.resource) continue;
      if(!first_buffer) std::cout << ',';first_buffer=false;
      std::cout << "{\"role\":\"" << item.second << "\",\"buffer_index\":" << i
                << ",\"length\":" << buffers[i].length
                << ",\"descriptor_offset\":" << item.first.byteOffset
                << ",\"descriptor_size\":" << item.first.byteSize;
      if(item.first.byteSize>0 && item.first.byteSize<=4096 &&
         item.first.byteOffset<=buffers[i].length &&
         item.first.byteSize<=buffers[i].length-item.first.byteOffset) {
        auto bytes=replay->GetBufferData(buffers[i].resourceId,item.first.byteOffset,item.first.byteSize);
        std::cout << ",\"small_descriptor_u32_le\":[";
        for(size_t j=0;j+4<=bytes.size();j+=4) {
          if(j) std::cout << ',';
          const uint32_t value=uint32_t(bytes[j]) | (uint32_t(bytes[j+1])<<8) |
              (uint32_t(bytes[j+2])<<16) | (uint32_t(bytes[j+3])<<24);
          std::cout << value;
        }
        std::cout << ']';
      }
      std::cout << '}';
      break;
    }
  }
  std::cout << "],\"potential_image_writes\":["; bool first_write=true;
  for(size_t i=0;i<textures.size();++i) {
    std::vector<EventUsage> writes;
    for(const auto& usage:replay->GetUsage(textures[i].resourceId)) {
      switch(usage.usage) {
        case ResourceUsage::ColorTarget: case ResourceUsage::CS_RWResource:
        case ResourceUsage::PS_RWResource: case ResourceUsage::All_RWResource:
        case ResourceUsage::CopyDst: case ResourceUsage::ResolveDst:
        case ResourceUsage::Copy: case ResourceUsage::Resolve:
        case ResourceUsage::Clear: case ResourceUsage::GenMips:
          writes.push_back(usage);break;
        default:break;
      }
    }
    if(writes.empty()) continue;
    if(!first_write) std::cout << ',';first_write=false;
    std::cout << "{\"texture_index\":" << i << ",\"width\":" << textures[i].width
              << ",\"height\":" << textures[i].height << ",\"events\":[";
    for(size_t j=0;j<writes.size();++j) {
      if(j) std::cout << ',';
      std::cout << "{\"event\":" << writes[j].eventId << ",\"kind\":" << unsigned(writes[j].usage) << '}';
    }
    std::cout << "]}";
  }
  std::cout << ']';
  if(inspect_mesh && selected && !selected_dispatch) {
    auto mesh=replay->GetPostVSData(0,0,MeshDataStage::VSOut);
    const bool readable=mesh.status.empty() && mesh.vertexResourceId!=ResourceId::Null() &&
        mesh.format.compType==CompType::Float && mesh.format.compByteWidth==4 &&
        mesh.format.compCount==4 && mesh.vertexByteStride>=16 && mesh.vertexByteStride<=4096;
    std::cout << ",\"postvs\":{\"instance\":0,\"view\":0,\"unproject\":"
              << (mesh.unproject?"true":"false") << ",\"elements\":" << mesh.numIndices
              << ",\"stride\":" << mesh.vertexByteStride << ",\"readable_float4\":"
              << (readable?"true":"false") << ",\"first_slots\":[";
    if(readable) {
      const auto count=std::min(mesh.numIndices,64U);
      auto bytes=replay->GetBufferData(mesh.vertexResourceId,mesh.vertexByteOffset,
                                      uint64_t(count)*mesh.vertexByteStride);
      bool first_slot=true;
      for(uint32_t slot=0;slot<count && uint64_t(slot)*mesh.vertexByteStride+16<=bytes.size();++slot) {
        if(!first_slot) std::cout << ',';first_slot=false;
        PixelValue position{};
        for(uint32_t c=0;c<4;++c) {
          const size_t j=size_t(slot)*mesh.vertexByteStride+c*4;
          const uint32_t bits=uint32_t(bytes[j]) | (uint32_t(bytes[j+1])<<8) |
              (uint32_t(bytes[j+2])<<16) | (uint32_t(bytes[j+3])<<24);
          std::memcpy(&position.floatValue[c],&bits,4);
        }
        Values(position,CompType::Float);
      }
    }
    std::cout << "]}";
  }
  std::cout << "}\n";
  return 0;
}
