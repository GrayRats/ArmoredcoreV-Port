#pragma once
// Opt-in CPU wall-time diagnostics for the ACV Vulkan command thread.
// Nested stages are inclusive and must not be added together.
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#if defined(__linux__)
#include <time.h>
#include <unistd.h>
#endif

namespace acv::gpu_profile {
enum class Stage : size_t { kFence, kResolve, kUpload, kDraw, kSubmit, kSlotAllocate, kReusePending, kReuseReady, kWaitFull, kWaitMissing, kWaitIncomplete, kPreviousReady, kInvalidate, kPublishCopy, kWaitPrevious, kFastSubmit, kGeneralResolve, kGeneralWait, kGeneralPublish, kGeneralSubmit, kGeneralFallback, kGeneralCpuWait, kGeneralPoolWait, kGeneralAdmission, kGeneralFittingSlot, kGeneralPoolDrain, kCount };
inline constexpr const char* kNames[] = {
    "submission_fence", "resolve_readback", "shared_upload", "draw", "end_submission",
    "slot_allocate", "slot_reuse_pending", "slot_reuse_ready", "wait_full",
    "wait_previous_missing", "wait_previous_incomplete", "previous_ready",
    "invalidate", "publish_copy", "wait_previous_generation", "fast_readback_submit", "general_resolve", "general_wait", "general_publish", "general_submit", "general_fallback", "general_cpu_wait", "general_pool_wait", "general_admission", "general_fitting_slot", "general_pool_drain"};
struct Config {
  const char* path = std::getenv("ACV_GPU_STAGE_PROFILE");
  double process_start = -1;
  Config() {
#if defined(__linux__)
    if (!path || !*path) return;
    std::ifstream stream("/proc/self/stat");
    std::string line;
    if (!std::getline(stream, line)) return;
    const auto end = line.rfind(')');
    if (end == std::string::npos) return;
    std::istringstream fields(line.substr(end + 2));
    std::string field;
    for (int i = 0; i <= 19; ++i) {
      if (!(fields >> field)) return;
    }
    try { process_start = std::stoull(field) / double(sysconf(_SC_CLK_TCK)); }
    catch (...) { process_start = -1; }
#endif
  }
  double age() const {
#if defined(__linux__)
    if (process_start < 0) return -1;
    timespec now{};
    if (clock_gettime(CLOCK_BOOTTIME, &now)) return -1;
    return now.tv_sec + now.tv_nsec / 1e9 - process_start;
#else
    return -1;
#endif
  }
};
inline const Config& configuration() { static const Config cfg; return cfg; }
struct State {
  struct Metric { unsigned long long calls = 0, ns = 0, max_ns = 0; };
  std::array<Metric, size_t(Stage::kCount)> metrics{};
  double since = -1;
  bool warned = false;
  void add(Stage stage, unsigned long long ns, double age) {
    if (since < 0) since = age;
    auto& value = metrics[size_t(stage)];
    ++value.calls; value.ns += ns;
    if (ns > value.max_ns) value.max_ns = ns;
    if (age - since < 5) return;
    std::ostringstream row;
    row << "{\"game_age_s\":" << age << ",\"window_s\":" << age-since
        << ",\"scope\":\"inclusive CPU wall time\",\"stages\":{";
    for (size_t i = 0; i < metrics.size(); ++i) {
      if (i) row << ',';
      const auto& m = metrics[i];
      row << '\"' << kNames[i] << "\":{\"calls\":" << m.calls
          << ",\"total_ms\":" << m.ns/1e6 << ",\"max_ms\":" << m.max_ns/1e6 << '}';
    }
    row << "}}\n";
    const std::string bytes = row.str();
    bool ok = false;
    if (FILE* file = std::fopen(configuration().path, "ab")) {
      ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
      if (std::fclose(file)) ok = false;
    }
    if (!ok && !warned) {
      std::fputs("[acv-gpu-profile] Cannot write stage diagnostics\n", stderr);
      warned = true;
    }
    metrics = {}; since = age;
  }
};
inline State& state() { static thread_local State data; return data; }
class Scope {
 public:
  explicit Scope(Stage stage) : stage_(stage) {
    const auto& cfg = configuration();
    active_ = cfg.path && *cfg.path && cfg.age() >= 90;
    if (active_) start_ = std::chrono::steady_clock::now();
  }
  ~Scope() {
    if (!active_) return;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start_).count();
    state().add(stage_, static_cast<unsigned long long>(ns), configuration().age());
  }
 private:
  Stage stage_;
  bool active_ = false;
  std::chrono::steady_clock::time_point start_{};
};
}  // namespace acv::gpu_profile
