// Host diagnostic: numeric render-target writers before one draw (no image export).
// Build with the same flags as tools/native-rexglue/replay_inspect.cpp.
// Usage: replay_targets CAPTURE.rdc TARGET_EVENT [WINDOW]
#include "renderdoc_replay.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <map>
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

struct DrawRecord { uint32_t event; uint32_t elements; std::vector<ResourceId> outputs; ResourceId depth; };

int main(int argc, char** argv) {
  if(argc<3 || argc>4) { std::cerr << "Usage: replay_targets CAPTURE.rdc TARGET_EVENT [WINDOW]\n"; return 2; }
  char* end=nullptr; const auto parsed=std::strtoul(argv[2],&end,10);
  if(end==argv[2] || *end!='\0' || parsed>UINT32_MAX) return 2;
  uint32_t target=static_cast<uint32_t>(parsed);
  const uint32_t window=argc==4 ? static_cast<uint32_t>(std::strtoul(argv[3],nullptr,10)) : 12;

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
  std::map<ResourceId,size_t> index;
  rdcarray<TextureDescription> textures=replay->GetTextures();
  for(size_t i=0;i<textures.size();++i) index[textures[i].resourceId]=i;

  if(target==0) {
    // Target 0 selects the last draw in the capture.
    for(auto a : actions) if((a->flags & ActionFlags::Drawcall)!=ActionFlags::NoFlags) target=a->eventId;
  }

  std::vector<DrawRecord> draws;
  for(auto a : actions) {
    if(a->eventId>target) break;
    if((a->flags & ActionFlags::Drawcall)==ActionFlags::NoFlags) continue;
    replay->SetFrameEvent(a->eventId,true);
    DrawRecord r{a->eventId,a->numIndices,{},ResourceId::Null()};
    for(auto id : a->outputs) if(id!=ResourceId::Null()) r.outputs.push_back(id);
    r.depth=a->depthOut;
    draws.push_back(r);
  }

  // Targets bound by the requested draw.
  const DrawRecord* last=nullptr;
  for(const auto& d : draws) if(d.event==target) last=&d;
  if(!last) { std::cerr << "Target is not a draw in this capture\n"; return 2; }

  std::cout << "{\"target_event\":" << target << ",\"target_elements\":" << last->elements << ",\"outputs\":[";
  for(size_t i=0;i<last->outputs.size();++i) {
    const auto& id=last->outputs[i];
    const auto it=index.find(id);
    if(it==index.end()) { std::cout << (i?",":"") << "{\"texture_index\":null}"; continue; }
    const auto& t=textures[it->second];
    std::cout << (i?",":"") << "{\"texture_index\":" << it->second << ",\"width\":" << t.width << ",\"height\":" << t.height << ",\"format\":\"" << unsigned(t.format.compType) << "\"}";
  }
  std::cout << "],\"depth_texture_index\":" << (index.count(last->depth)?index[last->depth]:-1) << ",\"writers\":{";

  // For each bound output: the most recent draws before target that wrote it.
  for(size_t i=0;i<last->outputs.size();++i) {
    const auto& id=last->outputs[i];
    std::vector<const DrawRecord*> writers;
    for(const auto& d : draws) {
      if(d.event>=target) break;
      for(auto o : d.outputs) if(o==id) { writers.push_back(&d); break; }
    }
    std::cout << (i?",":"") << "\"" << (index.count(id)?index[id]:-1) << "\":{\"count\":" << writers.size() << ",\"recent\":[";
    size_t start=writers.size()>window ? writers.size()-window : 0;
    for(size_t k=start;k<writers.size();++k)
      std::cout << (k>start?",":"") << "{\"event\":" << writers[k]->event << ",\"elements\":" << writers[k]->elements << "}";
    std::cout << "]}";
  }
  std::cout << "},\"buffers\":[";
  // EDRAM candidates: buffers of 10 MiB (Xenos EDRAM size) with usage up to the target.
  const auto& buffers=replay->GetBuffers();
  bool first_buffer=true;
  for(size_t b=0;b<buffers.size();++b) {
    if(buffers[b].length!=10485760ULL && buffers[b].length!=536870912ULL) continue;
    const auto usage=replay->GetUsage(buffers[b].resourceId);
    std::vector<EventUsage> before;
    uint32_t at_target=0;
    for(const auto& u : usage) {
      if(u.eventId==target) at_target|=1U<<0;
      if(u.eventId<=target) before.push_back(u);
    }
    std::map<uint32_t,uint32_t> counts;
    std::map<uint32_t,std::vector<uint32_t>> events_by_usage;
    for(const auto& u : before) {
      const auto code=static_cast<uint32_t>(u.usage);
      ++counts[code];
      events_by_usage[code].push_back(u.eventId);
    }
    std::cout << (first_buffer?"":",") << "{\"buffer_index\":" << b << ",\"usage_before_target\":" << before.size()
              << ",\"target_usage_bits\":" << at_target << ",\"counts\":{";
    first_buffer=false;
    bool first_count=true;
    for(const auto& c : counts) { std::cout << (first_count?"":",") << "\"" << c.first << "\":" << c.second; first_count=false; }
    std::cout << "},\"recent_by_usage\":{";
    bool first_kind=true;
    for(const auto& kind : events_by_usage) {
      const auto& v=kind.second;
      std::cout << (first_kind?"":",") << "\"" << kind.first << "\":[";
      first_kind=false;
      size_t start=v.size()>window ? v.size()-window : 0;
      for(size_t k=start;k<v.size();++k) std::cout << (k>start?",":"") << v[k];
      std::cout << "]";
    }
    std::cout << "},\"after_by_usage\":{";
    std::map<uint32_t,std::vector<uint32_t>> after;
    for(const auto& u : usage) if(u.eventId>target) after[static_cast<uint32_t>(u.usage)].push_back(u.eventId);
    bool first_after=true;
    for(const auto& kind : after) {
      std::cout << (first_after?"":",") << "\"" << kind.first << "\":{\"count\":" << kind.second.size() << ",\"first\":[";
      first_after=false;
      for(size_t k=0;k<kind.second.size() && k<window;++k) std::cout << (k?",":"") << kind.second[k];
      std::cout << "]}";
    }
    std::cout << "}}";
  }
  std::cout << "]}\n";
  return 0;
}
