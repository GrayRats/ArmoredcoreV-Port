// Host diagnostic: dump a buffer range as raw bytes at a chosen event (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_buffer_dump CAPTURE.rdc BUFFER_INDEX OFFSET LENGTH EVENT OUT_FILE
// BUFFER_INDEX is the local replay inventory index, or auto-shared for a unique 512 MiB buffer. The state is taken after EVENT.
#include "renderdoc_replay.h"
#include <cstdlib>
#include <cerrno>
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

static bool Parse(const char* text, uint64_t& out) {
  char* end = nullptr;
  errno = 0;
  out = std::strtoull(text, &end, 10);
  return !errno && text[0] != '-' && end != text && *end == '\0';
}

int main(int argc, char** argv) {
  if(argc != 7) { std::cerr << "Usage: replay_buffer_dump CAPTURE.rdc BUFFER_INDEX OFFSET LENGTH EVENT OUT_FILE\n"; return 2; }
  uint64_t buffer_index=UINT64_MAX, offset, length, event;
  const bool auto_shared=std::strcmp(argv[2],"auto-shared")==0;
  if((!auto_shared && !Parse(argv[2], buffer_index)) || !Parse(argv[3], offset) || !Parse(argv[4], length) ||
     !Parse(argv[5], event) || event > UINT32_MAX || length == 0) return 2;
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
  const rdcarray<BufferDescription>& buffers = replay->GetBuffers();
  if(auto_shared) {
    for(size_t i=0;i<buffers.size();++i) if(buffers[i].length==536870912ULL) {
      if(buffer_index!=UINT64_MAX) { std::cerr << "Multiple 512 MiB buffers; select an explicit index\n"; return 2; }
      buffer_index=i;
    }
  }
  if(buffer_index >= buffers.size() || offset > buffers[buffer_index].length ||
     length > buffers[buffer_index].length-offset) {
    std::cerr << "Range outside buffer\n"; return 2;
  }
  replay->SetFrameEvent(static_cast<uint32_t>(event), true);
  const bytebuf data = replay->GetBufferData(buffers[buffer_index].resourceId, offset, length);
  if(data.size()!=length) { std::cerr << "Incomplete buffer read\n"; return 1; }
  std::ofstream out(argv[6], std::ios::binary);
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  std::cout << "{\"bytes\":" << data.size() << ",\"event\":" << event << ",\"buffer_index\":" << buffer_index << "}\n";
  return out ? 0 : 1;
}
