// RenderDoc 1.46 replay-only GPU timestamps; no screenshot or live-game instrumentation.
// Build flags match replay_inspect.cpp. Replay timings are not live frame times.
#include "renderdoc_replay.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <map>
#include <set>
REPLAY_PROGRAM_MARKER();
struct Lifetime {
  Lifetime() { RENDERDOC_InitialiseReplay(GlobalEnvironment{}, {}); }
  ~Lifetime() { RENDERDOC_ShutdownReplay(); }
};
struct CaptureClose { void operator()(ICaptureFile* p) const { if(p) p->Shutdown(); } };
struct ReplayClose { void operator()(IReplayController* p) const { if(p) p->Shutdown(); } };
static void Walk(const rdcarray<ActionDescription>& actions,
                 std::map<uint32_t, const ActionDescription*>& leaves) {
  for(const auto& a : actions) {
    if(a.children.empty()) leaves.emplace(a.eventId, &a);
    else Walk(a.children, leaves);
  }
}
int main(int argc, char** argv) {
  if(argc != 2) { std::cerr << "Usage: replay_gpu_times CAPTURE.rdc\n"; return 2; }
  Lifetime lifetime;
  std::unique_ptr<ICaptureFile, CaptureClose> capture(RENDERDOC_OpenCaptureFile());
  if(!capture || !capture->OpenFile(argv[1], "rdc", nullptr).OK()) return 1;
  auto opened = capture->OpenCapture(ReplayOptions{}, nullptr);
  if(!opened.first.OK() || !opened.second) return 1;
  std::unique_ptr<IReplayController, ReplayClose> replay(opened.second);
  const auto counter = GPUCounter::EventGPUDuration;
  bool available = false;
  for(auto c : replay->EnumerateCounters()) if(c == counter) available = true;
  if(!available) { std::cerr << "GPU duration unavailable\n"; return 1; }
  auto desc = replay->DescribeCounter(counter);
  if(desc.resultType != CompType::Float || desc.unit != CounterUnit::Seconds ||
     (desc.resultByteWidth != 4 && desc.resultByteWidth != 8)) {
    std::cerr << "Unsupported duration representation\n"; return 1;
  }
  std::map<uint32_t, const ActionDescription*> leaves;
  Walk(replay->GetRootActions(), leaves);
  rdcarray<GPUCounter> counters; counters.push_back(counter);
  auto results = replay->FetchCounters(counters);
  std::set<uint32_t> seen;
  size_t valid = 0, skipped = 0;
  double total = 0;
  // Validate before emitting any result; parent marker durations are excluded.
  for(const auto& r : results) {
    if(r.counter != counter || !leaves.count(r.eventId)) { ++skipped; continue; }
    double seconds = desc.resultByteWidth == 8 ? r.value.d : r.value.f;
    if(!std::isfinite(seconds) || seconds < 0 || !seen.insert(r.eventId).second) {
      std::cerr << "Invalid or duplicate duration\n"; return 1;
    }
    ++valid; total += seconds;
  }
  if(!valid) { std::cerr << "No leaf event durations\n"; return 1; }
  std::cout << "{\"scope\":\"replay leaf events, not live frame time\",\"sum_ms\":"
            << total * 1000 << ",\"skipped\":" << skipped << ",\"events\":[";
  bool first = true;
  for(const auto& r : results) {
    if(r.counter != counter || !leaves.count(r.eventId)) continue;
    const auto& a = *leaves.at(r.eventId);
    const char* kind = (a.flags & ActionFlags::Drawcall) != ActionFlags::NoFlags ? "draw" :
        (a.flags & ActionFlags::Dispatch) != ActionFlags::NoFlags ? "dispatch" : "other";
    double seconds = desc.resultByteWidth == 8 ? r.value.d : r.value.f;
    std::cout << (first ? "" : ",") << "{\"event\":" << r.eventId
              << ",\"kind\":\"" << kind << "\",\"ms\":" << seconds * 1000 << '}';
    first = false;
  }
  std::cout << "]}\n";
}
