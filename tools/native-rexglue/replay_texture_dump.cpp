// Host diagnostic: dump the raw mip0 bytes of one replay texture at a chosen event (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_texture_dump CAPTURE.rdc TEXTURE_INDEX EVENT OUT_FILE
// TEXTURE_INDEX is the local GetTextures() index. Bytes are RenderDoc's linear block layout for
// block-compressed formats (not the guest tiling).
#include "renderdoc_replay.h"
#include <cstdlib>
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

int main(int argc, char** argv) {
  if(argc != 5) { std::cerr << "Usage: replay_texture_dump CAPTURE.rdc TEXTURE_INDEX EVENT OUT_FILE\n"; return 2; }
  char* end = nullptr;
  const size_t tex_index = std::strtoul(argv[2], &end, 10);
  if(end == argv[2] || *end != '\0') return 2;
  end = nullptr;
  const uint32_t event = static_cast<uint32_t>(std::strtoul(argv[3], &end, 10));
  if(end == argv[3] || *end != '\0') return 2;
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
  const rdcarray<TextureDescription> textures = replay->GetTextures();
  if(tex_index >= textures.size()) { std::cerr << "Texture index out of range\n"; return 2; }
  const TextureDescription& tex = textures[tex_index];
  replay->SetFrameEvent(event, true);
  const Subresource sub(0, 0, 0);
  const bytebuf data = replay->GetTextureData(tex.resourceId, sub);
  std::ofstream out(argv[4], std::ios::binary);
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  std::cout << "{\"texture_index\":" << tex_index << ",\"width\":" << tex.width << ",\"height\":" << tex.height
            << ",\"mips\":" << tex.mips << ",\"format_type\":" << static_cast<unsigned>(tex.format.type)
            << ",\"bytes\":" << data.size() << ",\"event\":" << event << "}\n";
  return out ? 0 : 1;
}
