// Host diagnostic: content of every colour/depth target after its last writer (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_target_content CAPTURE.rdc [MAX_EVENT]
// For each texture that an action lists in outputs/depthOut up to MAX_EVENT (default: last
// draw) prints writer count, last writer event, min/max and per-channel histogram mean over
// [0,1] (values outside the range are excluded by the replay API). texture_index is the local
// replay inventory index.
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

static void Walk(const rdcarray<ActionDescription>& actions, std::vector<const ActionDescription*>& flat) {
  for(const auto& a : actions) { flat.push_back(&a); Walk(a.children, flat); }
}
static void Values(const PixelValue& p, CompType type) {
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

struct Writer { uint32_t count = 0, last = 0, first = 0; bool depth = false; };

int main(int argc, char** argv) {
  if(argc<2 || argc>3) { std::cerr << "Usage: replay_target_content CAPTURE.rdc [MAX_EVENT]\n"; return 2; }
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
  uint32_t max_event=0;
  if(argc==3) max_event=static_cast<uint32_t>(std::strtoul(argv[2],nullptr,10));
  else for(auto a : actions) if((a->flags & ActionFlags::Drawcall)!=ActionFlags::NoFlags) max_event=a->eventId;

  std::map<ResourceId,Writer> writers;
  for(auto a : actions) {
    if(a->eventId>max_event) break;
    if((a->flags & ActionFlags::Drawcall)==ActionFlags::NoFlags) continue;
    for(auto id : a->outputs) {
      if(id==ResourceId::Null()) continue;
      auto& w=writers[id]; if(!w.count) w.first=a->eventId; ++w.count; w.last=a->eventId;
    }
    if(a->depthOut!=ResourceId::Null()) {
      auto& w=writers[a->depthOut]; if(!w.count) w.first=a->eventId; ++w.count; w.last=a->eventId; w.depth=true;
    }
  }

  const rdcarray<TextureDescription> textures=replay->GetTextures();
  std::cout << "{\"max_event\":" << max_event << ",\"targets\":[";
  bool first_target=true;
  for(const auto& entry : writers) {
    size_t index=textures.size();
    for(size_t i=0;i<textures.size();++i) if(textures[i].resourceId==entry.first) { index=i; break; }
    if(index==textures.size()) continue;
    const auto& tex=textures[index];
    replay->SetFrameEvent(entry.second.last,true);
    const Subresource sub(0,0,0);
    const auto range=replay->GetMinMax(tex.resourceId,sub,CompType::Typeless);
    std::cout << (first_target?"":",") << "{\"texture_index\":" << index << ",\"width\":" << tex.width << ",\"height\":" << tex.height
              << ",\"comp_type\":" << unsigned(tex.format.compType) << ",\"depth_target\":" << (entry.second.depth?"true":"false")
              << ",\"writers\":" << entry.second.count << ",\"first_writer\":" << entry.second.first
              << ",\"last_writer\":" << entry.second.last << ",\"min\":";
    first_target=false;
    Values(range.first,tex.format.compType);
    std::cout << ",\"max\":";
    Values(range.second,tex.format.compType);
    std::cout << ",\"center\":";
    Values(replay->PickPixel(tex.resourceId,tex.width/2,tex.height/2,sub,CompType::Typeless),tex.format.compType);
    if(!entry.second.depth && (tex.format.compType==CompType::UNorm || tex.format.compType==CompType::Float)) {
      std::cout << ",\"hist_mean_rgb\":[";
      for(int channel=0;channel<3;++channel) {
        rdcfixedarray<bool,4> channels={false,false,false,false}; channels[channel]=true;
        const auto histogram=replay->GetHistogram(tex.resourceId,sub,CompType::Typeless,0.0f,1.0f,channels);
        double total=0, weighted=0;
        for(size_t bin=0;bin<histogram.size();++bin) { total+=histogram[bin]; weighted+=double(bin)*histogram[bin]; }
        std::cout << (channel?",":"") << (total>0 && histogram.size()>1 ? weighted/total/double(histogram.size()-1) : 0.0);
      }
      std::cout << "]";
    }
    std::cout << "}";
  }
  std::cout << "]}\n";
  return 0;
}
