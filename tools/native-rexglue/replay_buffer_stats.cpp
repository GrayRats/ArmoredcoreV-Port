// Host diagnostic: numeric statistics of a buffer range at chosen events (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_buffer_stats CAPTURE.rdc BUFFER_INDEX OFFSET LENGTH EVENT [EVENT...]
// BUFFER_INDEX is the local replay inventory index (not a guest VA). Statistics are
// layout-independent: zero words, per-byte-lane max/mean and the share of words whose
// brightest lane exceeds a threshold. Lane order follows buffer byte order.
#include "renderdoc_replay.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
  char* end=nullptr; out=std::strtoull(text,&end,10);
  return end!=text && *end=='\0';
}

int main(int argc, char** argv) {
  if(argc<6) { std::cerr << "Usage: replay_buffer_stats CAPTURE.rdc BUFFER_INDEX OFFSET LENGTH EVENT [EVENT...]\n"; return 2; }
  uint64_t buffer_index, offset, length;
  if(!Parse(argv[2],buffer_index) || !Parse(argv[3],offset) || !Parse(argv[4],length) || length==0 || length%4) return 2;
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
  const rdcarray<BufferDescription>& buffers=replay->GetBuffers();
  if(buffer_index>=buffers.size() || offset+length>buffers[buffer_index].length) { std::cerr << "Range outside buffer\n"; return 2; }

  std::cout << "{\"buffer_index\":" << buffer_index << ",\"offset\":" << offset << ",\"length\":" << length << ",\"events\":[";
  for(int a=5;a<argc;++a) {
    uint64_t event;
    if(!Parse(argv[a],event) || event>UINT32_MAX) return 2;
    replay->SetFrameEvent(static_cast<uint32_t>(event),true);
    const bytebuf data=replay->GetBufferData(buffers[buffer_index].resourceId,offset,length);
    uint64_t zero_words=0, bright_words=0, sum[4]={0,0,0,0};
    uint8_t max_lane[4]={0,0,0,0};
    const uint64_t words=data.size()/4;
    for(uint64_t w=0;w<words;++w) {
      uint8_t peak=0; bool any=false;
      for(int lane=0;lane<4;++lane) {
        const uint8_t v=data[w*4+lane];
        sum[lane]+=v; max_lane[lane]=std::max(max_lane[lane],v); peak=std::max(peak,v); any|=v!=0;
      }
      if(!any) ++zero_words;
      if(peak>=128) ++bright_words;
    }
    std::cout << (a>5?",":"") << "{\"event\":" << event << ",\"bytes_read\":" << data.size() << ",\"words\":" << words
              << ",\"zero_words\":" << zero_words << ",\"words_peak_ge_128\":" << bright_words << ",\"lane_max\":["
              << unsigned(max_lane[0]) << ',' << unsigned(max_lane[1]) << ',' << unsigned(max_lane[2]) << ',' << unsigned(max_lane[3])
              << "],\"lane_mean\":[";
    for(int lane=0;lane<4;++lane) std::cout << (lane?",":"") << (words ? double(sum[lane])/double(words) : 0.0);
    std::cout << "]}";
  }
  std::cout << "]}\n";
  return 0;
}
