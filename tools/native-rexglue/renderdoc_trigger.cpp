// Timed RenderDoc frame captures for diagnostics; inactive unless RenderDoc is
// already loaded into the process (LD_PRELOAD + ENABLE_VULKAN_RENDERDOC_CAPTURE=1).
// ACV_RENDERDOC_CAPTURE_SECONDS: optional comma-separated seconds after start, e.g. "60,90".
// ACV_RENDERDOC_CAPTURE_PATH: capture file path template (RenderDoc appends frame info).
// ACV_RENDERDOC_CAPTURE_FRAMES: 1..8 consecutive presents per trigger (default 1).
// Uses only the public in-application API from renderdoc_app.h (API 1.6.0).
#include <renderdoc_app.h>

#include <dlfcn.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

void RunTriggers(RENDERDOC_API_1_6_0* api, std::vector<int> seconds, uint32_t frames) {
  const auto start = std::chrono::steady_clock::now();
  for (int second : seconds) {
    std::this_thread::sleep_until(start + std::chrono::seconds(second));
    if (api->IsFrameCapturing()) {
      std::fprintf(stderr, "[acv-renderdoc] Capture already active at %d s; skipping\n", second);
      continue;
    }
    if(frames==1) api->TriggerCapture();
    else api->TriggerMultiFrameCapture(frames);
    std::fprintf(stderr, "[acv-renderdoc] TriggerCapture at %d s (%u frame(s))\n", second,frames);
  }
}

struct RenderDocTrigger {
  RenderDocTrigger() {
    const char* schedule = std::getenv("ACV_RENDERDOC_CAPTURE_SECONDS");
    const char* path = std::getenv("ACV_RENDERDOC_CAPTURE_PATH");
    if ((schedule == nullptr || *schedule == '\0') && path == nullptr) {
      return;
    }
    // Preloaded (LD_PRELOAD) or loaded by the Vulkan layer: resolve from the global scope.
    auto get_api = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI"));
    if (get_api == nullptr) {
      std::fprintf(stderr, "[acv-renderdoc] RENDERDOC_GetAPI not found; captures disabled\n");
      return;
    }
    RENDERDOC_API_1_6_0* api = nullptr;
    if (get_api == nullptr || get_api(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&api)) != 1) {
      std::fprintf(stderr, "[acv-renderdoc] RENDERDOC_GetAPI 1.6.0 unavailable; captures disabled\n");
      return;
    }
    if (path != nullptr) {
      api->SetCaptureFilePathTemplate(path);
      std::fprintf(stderr, "[acv-renderdoc] capture path template: %s\n", path);
    }
    if (schedule == nullptr || *schedule == '\0') {
      return;  // manual captures only (F12 in the game window)
    }
    uint32_t frames=1;
    if(const char* count=std::getenv("ACV_RENDERDOC_CAPTURE_FRAMES")) {
      char* end=nullptr;const auto parsed=std::strtoul(count,&end,10);
      if(end==count || *end!='\0' || parsed<1 || parsed>8) {
        std::fprintf(stderr,"[acv-renderdoc] Invalid capture frame count; automatic captures disabled\n");
        return;
      }
      frames=static_cast<uint32_t>(parsed);
    }
    std::vector<int> seconds;
    std::stringstream stream(schedule);
    for (std::string item; std::getline(stream, item, ',');) {
      seconds.push_back(std::atoi(item.c_str()));
    }
    std::fprintf(stderr, "[acv-renderdoc] scheduled %zu capture(s): %s\n", seconds.size(), schedule);
    std::thread(RunTriggers, api, std::move(seconds),frames).detach();
  }
};

const RenderDocTrigger g_renderdoc_trigger;

}  // namespace
