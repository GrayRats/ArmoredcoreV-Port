// Host diagnostic: compute shader reflection and disassembly at chosen events (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_dispatch_shader CAPTURE.rdc OUT_DIR EVENT [EVENT...]
// Prints JSON with the resources the shader declares (isReadOnly comes from the SPIR-V
// NonWritable decoration) and writes OUT_DIR/event_N.disasm.txt for each event.
#include "renderdoc_replay.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
REPLAY_PROGRAM_MARKER();

struct ReplayLifetime {
  ReplayLifetime() { RENDERDOC_InitialiseReplay(GlobalEnvironment{}, {}); }
  ~ReplayLifetime() { RENDERDOC_ShutdownReplay(); }
};
struct CloseCapture { void operator()(ICaptureFile* p) const { if(p) p->Shutdown(); } };
struct CloseReplay { void operator()(IReplayController* p) const { if(p) p->Shutdown(); } };

static void Resources(const char* key, const rdcarray<ShaderResource>& list) {
  std::cout << ",\"" << key << "\":[";
  for(size_t i=0;i<list.size();++i) {
    const auto& r=list[i];
    std::cout << (i?",":"") << "{\"name\":\"" << std::string(r.name.c_str()) << "\",\"set\":" << r.fixedBindSetOrSpace
              << ",\"binding\":" << r.fixedBindNumber << ",\"read_only\":" << (r.isReadOnly?"true":"false")
              << ",\"texture\":" << (r.isTexture?"true":"false")
              << ",\"descriptor_type\":" << unsigned(r.descriptorType) << "}";
  }
  std::cout << "]";
}

int main(int argc, char** argv) {
  if(argc<4) { std::cerr << "Usage: replay_dispatch_shader CAPTURE.rdc OUT_DIR EVENT [EVENT...]\n"; return 2; }
  const std::string out_dir=argv[2];
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

  const rdcarray<rdcstr> targets=replay->GetDisassemblyTargets(true);
  std::cout << "{\"disassembly_targets\":[";
  for(size_t i=0;i<targets.size();++i) std::cout << (i?",":"") << "\"" << std::string(targets[i].c_str()) << "\"";
  std::cout << "],\"events\":[";
  for(int a=3;a<argc;++a) {
    char* end=nullptr; const auto parsed=std::strtoul(argv[a],&end,10);
    if(end==argv[a] || *end!='\0' || parsed>UINT32_MAX) return 2;
    const uint32_t event=static_cast<uint32_t>(parsed);
    replay->SetFrameEvent(event,true);
    const VKPipe::State* vk=replay->GetVulkanPipelineState();
    std::cout << (a>3?",":"") << "{\"event\":" << event;
    if(!vk || !vk->computeShader.reflection) { std::cout << ",\"error\":\"no compute reflection\"}"; continue; }
    const ShaderReflection* refl=vk->computeShader.reflection;
    std::cout << ",\"entry\":\"" << std::string(vk->computeShader.entryPoint.c_str()) << "\"";
    std::cout << ",\"threads\":[" << refl->dispatchThreadsDimension[0] << ',' << refl->dispatchThreadsDimension[1]
              << ',' << refl->dispatchThreadsDimension[2] << "]";
    Resources("read_only_resources",refl->readOnlyResources);
    Resources("read_write_resources",refl->readWriteResources);
    std::cout << ",\"push_constant_bytes\":" << vk->pushconsts.size();
    // Bound resources per descriptor access: reflection index -> local buffer inventory index.
    const rdcarray<BufferDescription>& buffers=replay->GetBuffers();
    std::cout << ",\"bindings\":[";
    bool first_binding=true;
    for(const auto& access : replay->GetDescriptorAccess()) {
      if(access.stage!=ShaderStage::Compute) continue;
      const rdcarray<Descriptor> descriptors=replay->GetDescriptors(access.descriptorStore,{DescriptorRange(access)});
      if(descriptors.empty()) continue;
      long buffer_index=-1;
      for(size_t b=0;b<buffers.size();++b) if(buffers[b].resourceId==descriptors[0].resource) buffer_index=static_cast<long>(b);
      std::cout << (first_binding?"":",") << "{\"descriptor_type\":" << unsigned(access.type) << ",\"reflection_index\":" << access.index
                << ",\"buffer_index\":" << buffer_index << ",\"offset\":" << descriptors[0].byteOffset
                << ",\"size\":" << descriptors[0].byteSize << "}";
      first_binding=false;
    }
    std::cout << "]";
    const std::string target=targets.empty() ? std::string() : std::string(targets[0].c_str());
    const rdcstr text=replay->DisassembleShader(vk->compute.pipelineResourceId,refl,rdcstr(target.c_str()));
    std::ofstream(out_dir+"/event_"+std::to_string(event)+".disasm.txt") << text.c_str();
    std::cout << ",\"disasm_bytes\":" << text.size() << "}";
  }
  std::cout << "]}\n";
  return 0;
}
