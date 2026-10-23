// Host diagnostic: per-texture write/read usage from a RenderDoc capture (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_texture_usage CAPTURE.rdc
// texture_index is local to this replay inventory, not a guest VA. Usage codes are
// the ResourceUsage enum in replay_enums.h (17 PS_Resource, 27 CS_RWResource,
// 32 ColorTarget, 40 ResolveDst, 43 CopyDst, 35 Clear).
#include "renderdoc_replay.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
REPLAY_PROGRAM_MARKER();

struct ReplayLifetime {
  ReplayLifetime() { RENDERDOC_InitialiseReplay(GlobalEnvironment{}, {}); }
  ~ReplayLifetime() { RENDERDOC_ShutdownReplay(); }
};
struct CloseCapture { void operator()(ICaptureFile* p) const { if(p) p->Shutdown(); } };
struct CloseReplay { void operator()(IReplayController* p) const { if(p) p->Shutdown(); } };

struct Span { uint32_t count = 0, first = 0, last = 0; };

int main(int argc, char** argv) {
  if(argc!=2) { std::cerr << "Usage: replay_texture_usage CAPTURE.rdc\n"; return 2; }
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

  const rdcarray<TextureDescription> textures=replay->GetTextures();
  std::cout << "{\"textures\":[";
  bool first_texture=true;
  for(size_t i=0;i<textures.size();++i) {
    const auto usage=replay->GetUsage(textures[i].resourceId);
    if(usage.empty()) continue;
    std::map<uint32_t,Span> spans;
    for(const auto& u : usage) {
      auto& s=spans[static_cast<uint32_t>(u.usage)];
      if(!s.count || u.eventId<s.first) s.first=u.eventId;
      if(u.eventId>s.last) s.last=u.eventId;
      ++s.count;
    }
    std::cout << (first_texture?"":",") << "{\"texture_index\":" << i << ",\"width\":" << textures[i].width
              << ",\"height\":" << textures[i].height << ",\"comp_type\":" << unsigned(textures[i].format.compType)
              << ",\"usage\":{";
    first_texture=false;
    bool first_code=true;
    for(const auto& s : spans) {
      std::cout << (first_code?"":",") << "\"" << s.first << "\":[" << s.second.count << ',' << s.second.first << ',' << s.second.last << "]";
      first_code=false;
    }
    std::cout << "}}";
  }
  std::cout << "]}\n";
  return 0;
}
