// Host diagnostic: graphics shader reflection, bindings, constant values and disassembly for one draw.
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_draw_shaders CAPTURE.rdc OUT_DIR EVENT
// Writes OUT_DIR/event_N.vs.txt and OUT_DIR/event_N.fs.txt, prints JSON to stdout. Numbers only.
#include "renderdoc_replay.h"
#include <cmath>
#include <cstring>
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

static void Walk(const rdcarray<ActionDescription>& actions, std::vector<const ActionDescription*>& flat) {
  for(const auto& a : actions) { flat.push_back(&a); Walk(a.children, flat); }
}

static void Stage(IReplayController* replay, const char* name, const VKPipe::Shader& sh, ResourceId pipeline,
                  const std::string& out_path, const rdcstr& disasm_target) {
  std::cout << "\"" << name << "\":{";
  if(sh.resourceId == ResourceId::Null() || !sh.reflection) { std::cout << "\"present\":false}"; return; }
  const ShaderReflection* refl = sh.reflection;
  std::cout << "\"present\":true,\"entry\":\"" << std::string(sh.entryPoint.c_str()) << "\"";
  std::cout << ",\"read_only\":[";
  for(size_t i = 0; i < refl->readOnlyResources.size(); ++i) {
    const auto& r = refl->readOnlyResources[i];
    std::cout << (i ? "," : "") << "{\"name\":\"" << std::string(r.name.c_str()) << "\",\"set\":" << r.fixedBindSetOrSpace
              << ",\"binding\":" << r.fixedBindNumber << ",\"texture\":" << (r.isTexture ? "true" : "false") << "}";
  }
  std::cout << "],\"read_write\":[";
  for(size_t i = 0; i < refl->readWriteResources.size(); ++i) {
    const auto& r = refl->readWriteResources[i];
    std::cout << (i ? "," : "") << "{\"name\":\"" << std::string(r.name.c_str()) << "\",\"set\":" << r.fixedBindSetOrSpace
              << ",\"binding\":" << r.fixedBindNumber << "}";
  }
  std::cout << "],\"constant_blocks\":[";
  for(size_t i = 0; i < refl->constantBlocks.size(); ++i) {
    const auto& cb = refl->constantBlocks[i];
    std::cout << (i ? "," : "") << "{\"name\":\"" << std::string(cb.name.c_str()) << "\",\"set\":" << cb.fixedBindSetOrSpace
              << ",\"binding\":" << cb.fixedBindNumber << ",\"byte_size\":" << cb.byteSize << "}";
  }
  std::cout << "]";
  const rdcstr text = replay->DisassembleShader(pipeline, refl, disasm_target);
  std::ofstream(out_path) << text.c_str();
  std::cout << ",\"disasm_bytes\":" << text.size() << "}";
}

int main(int argc, char** argv) {
  if(argc != 4) { std::cerr << "Usage: replay_draw_shaders CAPTURE.rdc OUT_DIR EVENT\n"; return 2; }
  char* end = nullptr;
  const uint32_t event = static_cast<uint32_t>(std::strtoul(argv[3], &end, 10));
  if(end == argv[3] || *end != '\0') return 2;
  const std::string out_dir = argv[2];
  ReplayLifetime lifetime;
  std::unique_ptr<ICaptureFile, CloseCapture> capture(RENDERDOC_OpenCaptureFile());
  if(!capture) { std::cerr << "OpenCaptureFile returned null\n"; return 1; }
  auto opened = capture->OpenFile(argv[1], "rdc", nullptr);
  if(!opened.OK()) { std::cerr << static_cast<unsigned>(opened.code) << '\n'; return 1; }
  auto replayed = capture->OpenCapture(ReplayOptions{}, nullptr);
  if(!replayed.first.OK() || !replayed.second) {
    std::cerr << static_cast<unsigned>(replayed.first.code) << '\n'; return 1;
  }
  std::unique_ptr<IReplayController, CloseReplay> replay(replayed.second);
  std::vector<const ActionDescription*> actions;
  Walk(replay->GetRootActions(), actions);
  const ActionDescription* draw = nullptr;
  for(auto a : actions) if(a->eventId == event) draw = a;
  if(!draw) { std::cerr << "Event is not an action\n"; return 2; }
  replay->SetFrameEvent(event, true);
  const VKPipe::State* vk = replay->GetVulkanPipelineState();
  if(!vk) { std::cerr << "No Vulkan state\n"; return 1; }
  const rdcarray<BufferDescription>& buffers = replay->GetBuffers();
  const rdcarray<rdcstr> targets = replay->GetDisassemblyTargets(true);
  const rdcstr target = targets.empty() ? rdcstr("SPIR-V (RenderDoc)") : targets[0];

  std::cout << "{\"event\":" << event << ",\"elements\":" << draw->numIndices << ",\"outputs\":[";
  for(size_t i = 0; i < draw->outputs.size(); ++i) std::cout << (i ? "," : "") << "\"out" << i << "\"";
  std::cout << "],";
  Stage(replay.get(), "vertex", vk->vertexShader, vk->graphics.pipelineResourceId, out_dir + "/event_" + std::to_string(event) + ".vs.txt", target);
  std::cout << ",";

  // Fragment-stage constant buffer bindings with their current bytes (as floats in the output).
  const rdcarray<DescriptorAccess>& accesses = replay->GetDescriptorAccess();
  std::cout << "\"fragment_bindings\":[";
  bool first = true;
  for(const auto& access : accesses) {
    if(access.stage != ShaderStage::Pixel) continue;
    const rdcarray<Descriptor> descs = replay->GetDescriptors(access.descriptorStore, {DescriptorRange(access)});
    if(descs.empty()) continue;
    long buffer_index = -1;
    for(size_t b = 0; b < buffers.size(); ++b) if(buffers[b].resourceId == descs[0].resource) buffer_index = static_cast<long>(b);
    std::cout << (first ? "" : ",") << "{\"type\":" << unsigned(access.type) << ",\"reflection_index\":" << access.index
              << ",\"buffer_index\":" << buffer_index << ",\"offset\":" << descs[0].byteOffset << ",\"size\":" << descs[0].byteSize;
    if(buffer_index >= 0 && descs[0].byteSize > 0 && descs[0].byteSize <= 4096) {
      const bytebuf bytes = replay->GetBufferData(buffers[buffer_index].resourceId, descs[0].byteOffset, descs[0].byteSize);
      std::cout << ",\"floats\":[";
      for(size_t i = 0; i + 4 <= bytes.size(); i += 4) {
        float f;
        std::memcpy(&f, bytes.data() + i, 4);
        std::cout << (i ? "," : "") << (std::isfinite(f) ? f : 0.0f);
      }
      std::cout << "]";
    }
    std::cout << "}";
    first = false;
  }
  std::cout << "],";
  Stage(replay.get(), "fragment", vk->fragmentShader, vk->graphics.pipelineResourceId, out_dir + "/event_" + std::to_string(event) + ".fs.txt", target);
  std::cout << "}\n";
  return 0;
}
