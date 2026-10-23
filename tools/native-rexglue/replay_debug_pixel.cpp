// Host diagnostic: run RenderDoc's pixel shader debugger for one pixel of one draw and write the
// per-step variable changes as numeric lines (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_debug_pixel CAPTURE.rdc EVENT X Y OUT_FILE
//        replay_debug_pixel CAPTURE.rdc EVENT W H STEP
// Scan mode prints covered pixels. Diagnostic replay flags:
// --debug-relax-rte: remove the explicit RTE requirement for RenderDoc's interpreter.
// --debug-relax-fsi: also remove interlock; only pre-EDRAM arithmetic is meaningful.
// --debug-fix-fetch-exp: additionally correct the diagnosed texture exponent word
//   in a saved old capture. This never patches the game/runtime.
// ACV_REPLAY_PRIMITIVE=N requests a primitive, but always compare actual input
// values: selection can still differ for storage-writing shaders in replay.
// Output file: "INPUT name=v0,v1,.." lines, then "STEP n name=v0,.. (before->after)" per change.
#include "renderdoc_replay.h"
#include <spirv/unified1/spirv.hpp>
#include "replay_fetch_exponent_patch.h"
#include <vector>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
REPLAY_PROGRAM_MARKER();

struct ReplayLifetime {
  ReplayLifetime() { RENDERDOC_InitialiseReplay(GlobalEnvironment{}, {}); }
  ~ReplayLifetime() { RENDERDOC_ShutdownReplay(); }
};
struct CloseCapture { void operator()(ICaptureFile* p) const { if(p) p->Shutdown(); } };
struct CloseReplay { void operator()(IReplayController* p) const { if(p) p->Shutdown(); } };

static void WriteValue(std::ofstream& out, const ShaderVariable& v) {
  out << v.name.c_str() << "=";
  for(uint32_t c = 0; c < v.columns && c < 4; ++c) {
    if(c) out << ",";
    if(v.type == VarType::Float) out << v.value.f32v[c];
    else if(v.type == VarType::UInt) out << v.value.u32v[c];
    else if(v.type == VarType::SInt) out << v.value.s32v[c];
    else out << "?";
  }
}

int main(int argc, char** argv) {
  if(argc != 6 && argc != 7) { std::cerr << "Usage: replay_debug_pixel CAPTURE.rdc EVENT X Y OUT_FILE | W H STEP\n"; return 2; }
  char* end = nullptr;
  const uint32_t event = static_cast<uint32_t>(std::strtoul(argv[2], &end, 10));
  if(end == argv[2] || *end) return 2;
  const uint32_t x = static_cast<uint32_t>(std::strtoul(argv[3], &end, 10));
  if(end == argv[3] || *end) return 2;
  const uint32_t y = static_cast<uint32_t>(std::strtoul(argv[4], &end, 10));
  if(end == argv[4] || *end) return 2;
  // Replay-only probe. Disabling interlock is NOT valid rendering semantics;
  // use only pre-EDRAM shader arithmetic, never as an output/parity oracle.
  const bool fix_fetch = argc == 7 && std::strcmp(argv[6], "--debug-fix-fetch-exp") == 0;
  const bool relax_fsi = fix_fetch || (argc == 7 && std::strcmp(argv[6], "--debug-relax-fsi") == 0);
  const bool relax_fp = relax_fsi || (argc == 7 && std::strcmp(argv[6], "--debug-relax-rte") == 0);
  if(argc == 7 && !relax_fp) return 2;
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
  replay->SetFrameEvent(event, true);
  const VKPipe::State* vk = replay->GetVulkanPipelineState();
  auto report_pixel = [&](const char* label) {
    const VKPipe::State* state = replay->GetVulkanPipelineState();
    if(!state || std::strstr(argv[5], ".txt") == nullptr ||
       state->currentPass.renderpass.colorAttachments.empty()) return;
    const uint32_t index = state->currentPass.renderpass.colorAttachments[0];
    if(index >= state->currentPass.framebuffer.attachments.size()) return;
    const Descriptor& desc = state->currentPass.framebuffer.attachments[index];
    if(desc.resource == ResourceId::Null()) return;
    const PixelValue value = replay->PickPixel(desc.resource, x, y,
        Subresource(desc.firstMip, desc.firstSlice, 0), CompType::Typeless);
    std::cerr << label << " color0=" << value.floatValue[0] << "," << value.floatValue[1]
              << "," << value.floatValue[2] << "," << value.floatValue[3] << "\n";
  };
  report_pixel("Captured shader");
  struct Replacement {
    IReplayController* replay;
    ResourceId original, replacement;
    ~Replacement() {
      if(replacement != ResourceId::Null()) {
        replay->RemoveReplacement(original);
        replay->FreeTargetResource(replacement);
      }
    }
  } replacement{replay.get(), ResourceId::Null(), ResourceId::Null()};
  if(relax_fp && vk && vk->fragmentShader.reflection) {
    // Diagnostic replay only. Removing the explicit RTE requirement lets RenderDoc 1.46
    // interpret this module. This is not a runtime fix or a floating-point parity oracle.
    const ShaderReflection* reflection = vk->fragmentShader.reflection;
    const bytebuf& raw = reflection->rawBytes;
    if(raw.size() < 20 || raw.size() % 4) return 2;
    std::vector<uint32_t> words(raw.size() / 4);
    std::memcpy(words.data(), raw.data(), raw.size());
    if(words[0] != spv::MagicNumber) return 2;
    std::vector<uint32_t> filtered(words.begin(), words.begin() + 5);
    uint32_t removed = 0;
    for(size_t i = 5; i < words.size();) {
      const uint32_t count = words[i] >> 16, op = words[i] & 0xFFFF;
      if(!count || count > words.size() - i) return 2;
      const bool rte = (op == spv::OpCapability && count == 2 &&
                        words[i+1] == spv::CapabilityRoundingModeRTE) ||
                       (op == spv::OpExecutionMode && count >= 3 &&
                        words[i+2] == spv::ExecutionModeRoundingModeRTE);
      const bool interlock = relax_fsi && (
          (op == spv::OpExtension && count > 1 &&
           std::strcmp(reinterpret_cast<const char*>(&words[i+1]), "SPV_EXT_fragment_shader_interlock") == 0) ||
          (op == spv::OpCapability && count == 2 &&
           (words[i+1] == spv::CapabilityFragmentShaderSampleInterlockEXT ||
            words[i+1] == spv::CapabilityFragmentShaderPixelInterlockEXT)) ||
          (op == spv::OpExecutionMode && count >= 3 &&
           (words[i+2] == spv::ExecutionModeSampleInterlockOrderedEXT ||
            words[i+2] == spv::ExecutionModePixelInterlockOrderedEXT)) ||
          op == spv::OpBeginInvocationInterlockEXT || op == spv::OpEndInvocationInterlockEXT);
      if(rte || interlock) ++removed;
      else filtered.insert(filtered.end(), words.begin() + i, words.begin() + i + count);
      i += count;
    }
    if(fix_fetch) {
      try {std::cerr << "Replay texture exponents corrected: " << FixReplayFetchExponent(filtered) << "\n";}
      catch(const std::exception& e) {std::cerr << e.what() << "\n";return 2;}
    }
    bytebuf source;
    source.resize(filtered.size() * 4);
    std::memcpy(source.data(), filtered.data(), source.size());
    const rdcstr entry = vk->fragmentShader.entryPoint;
    replacement.original = vk->fragmentShader.resourceId;
    auto built = replay->BuildTargetShader(entry, ShaderEncoding::SPIRV, source,
                                          ShaderCompileFlags{}, ShaderStage::Pixel);
    if(built.first == ResourceId::Null()) {
      std::cerr << "Replacement build failed: " << built.second.c_str() << "\n";
      return 1;
    }
    replacement.replacement = built.first;
    replay->ReplaceResource(replacement.original, replacement.replacement);
    replay->SetFrameEvent(event, true);
    vk = replay->GetVulkanPipelineState();
    report_pixel("Diagnostic shader");
    std::cerr << "Diagnostic declarations/instructions removed: " << removed << "\n";
  }
  if(vk && vk->fragmentShader.reflection && !vk->fragmentShader.reflection->debugInfo.debuggable) {
    std::cerr << "Shader is not debuggable: " << vk->fragmentShader.reflection->debugInfo.debugStatus.c_str() << "\n";
    return 4;
  }
  if(vk) {
    for(const auto& v : vk->viewportScissor.viewportScissors) {
      std::cerr << "Viewport " << v.vp.x << "," << v.vp.y << " "
                << v.vp.width << "x" << v.vp.height << "\n";
    }
  }
  const bool scan_mode = std::strstr(argv[5], ".txt") == nullptr;
  if(scan_mode) {
    // Scan mode: args are W H STEP; print every grid pixel that the debugger covers.
    const uint32_t width = x, height = y;
    const uint32_t step = static_cast<uint32_t>(std::strtoul(argv[5], &end, 10));
    if(end == argv[5] || *end || !step || !width || !height) return 2;
    uint32_t covered = 0;
    for(uint32_t sy = 0; sy < height; sy += step) {
      for(uint32_t sx = 0; sx < width; sx += step) {
        DebugPixelInputs probe;
        ShaderDebugTrace* t = replay->DebugPixel(sx, sy, probe);
        if(t && t->debugger) { ++covered; std::cout << "COVERED " << sx << " " << sy << "\n"; replay->FreeTrace(t); }
        else if(t) replay->FreeTrace(t);
      }
    }
    std::cout << "{\"event\":" << event << ",\"covered\":" << covered << "}\n";
    return 0;
  }
  DebugPixelInputs inputs;
  // Replaying a storage-writing shader may change which overlapping fragment
  // the debugger chooses. Pin the primitive for a same-input comparison.
  if(const char* primitive=std::getenv("ACV_REPLAY_PRIMITIVE")) {
    char* tail=nullptr;const auto value=std::strtoull(primitive,&tail,10);
    if(!*primitive || *primitive=='-' || tail==primitive || *tail || value>UINT32_MAX)return 2;
    inputs.primitive=uint32_t(value);
    std::cerr << "Diagnostic primitive: " << inputs.primitive << "\n";
  }
  ShaderDebugTrace* trace = replay->DebugPixel(x, y, inputs);
  if(!trace || !trace->debugger) {
    std::cerr << "DebugPixel returned no trace (pixel outside draw or unsupported)\n";
    return 3;
  }
  std::ofstream out(argv[5]);
  for(const ShaderVariable& v : trace->inputs) { out << "INPUT "; WriteValue(out, v); out << "\n"; }
  size_t steps = 0;
  for(;;) {
    rdcarray<ShaderDebugState> states = replay->ContinueDebug(trace->debugger);
    if(states.empty()) break;
    for(const ShaderDebugState& st : states) {
      ++steps;
      for(const ShaderVariableChange& ch : st.changes) {
        out << "STEP " << steps << " ip=" << st.nextInstruction << " ";
        WriteValue(out, ch.after);
        out << "\n";
      }
    }
    if(steps > 2000000) break;
  }
  const size_t input_count = trace->inputs.size();
  replay->FreeTrace(trace);
  std::cout << "{\"event\":" << event << ",\"x\":" << x << ",\"y\":" << y << ",\"inputs\":" << input_count
            << ",\"steps\":" << steps << "}\n";
  return 0;
}
