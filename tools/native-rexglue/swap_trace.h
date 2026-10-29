#pragma once
// Bounded opt-in swap timestamps. CPU wall time only, not GPU timestamps or
// unique presented frames. No file writes or allocations per recorded swap.
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#if defined(__linux__)
#include <time.h>
#include <unistd.h>
#endif

namespace acv::swap_trace {
using Clock = std::chrono::steady_clock;
inline unsigned long long ticks(Clock::time_point value) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch()).count();
}
struct Options {
  const char* path = nullptr;
  double after_s = 120, duration_s = 60;
  bool valid() const {
    return path && *path && std::isfinite(after_s) && after_s >= 90 && after_s <= 3600 &&
           std::isfinite(duration_s) && duration_s >= 1 && duration_s <= 120;
  }
};
template <size_t Capacity = 16384>
class Recorder {
 public:
  explicit Recorder(Options options) : options_(options) { static_assert(Capacity >= 2); }
  ~Recorder() { if (size_ && !stopped_) flush("shutdown", false); }
  bool ready(double age) const {
    return !stopped_ && options_.valid() && std::isfinite(age) && age >= options_.after_s;
  }
  void add(unsigned long long arrival, unsigned long long paced,
           unsigned long long done, double age) {
    if (!ready(age)) return;
    if (arrival > paced || paced > done || (size_ && rows_[size_-1].done > arrival)) {
      flush("invalid_clock_order", false); return;
    }
    if (!size_) since_ = age;
    rows_[size_++] = {arrival, paced, done, age};
    if (age - since_ >= options_.duration_s) flush("duration", true);
    else if (size_ == Capacity) flush("capacity", false);
  }
  bool stopped() const { return stopped_; }
 private:
  struct Row { unsigned long long arrival, paced, done; double age; };
  Options options_;
  std::array<Row, Capacity> rows_{};
  size_t size_ = 0;
  double since_ = -1;
  bool stopped_ = false;
  void flush(const char* reason, bool complete) {
    stopped_ = true;  // One attempt. Never retry writes on the command thread.
    FILE* file = std::fopen(options_.path, "wx");
    bool ok = file != nullptr;
    if (file) {
      ok = std::fprintf(file,
          "{\"kind\":\"metadata\",\"schema\":1,\"samples\":%zu,\"complete\":%s,"
          "\"stop_reason\":\"%s\",\"scope\":\"CPU guest swap boundary; not unique presents or GPU timestamps\"}\n",
          size_, complete ? "true" : "false", reason) > 0;
      for (size_t i=0; ok && i<size_; ++i) {
        const auto& r = rows_[i];
        ok = std::fprintf(file,
            "{\"kind\":\"swap\",\"age_s\":%.9f,\"arrival_ns\":%llu,\"paced_ns\":%llu,\"done_ns\":%llu}\n",
            r.age, r.arrival, r.paced, r.done) > 0;
      }
      if (std::fclose(file)) ok = false;
    }
    if (!ok) std::fputs("[acv-swap-trace] Cannot write exclusive diagnostic file\n", stderr);
  }
};
struct Runtime {
  Options options;
  double process_start = -1;
  std::unique_ptr<Recorder<>> recorder;
  Runtime() {
    options.path = std::getenv("ACV_SWAP_TRACE");
    if (!options.path || !*options.path) return;
    auto number = [](const char* name, double fallback) {
      const char* text = std::getenv(name);
      if (!text) return fallback;
      char* end = nullptr;
      double value = std::strtod(text, &end);
      return end != text && !*end ? value : -1.0;
    };
    options.after_s = number("ACV_SWAP_TRACE_AFTER_S", 120);
    options.duration_s = number("ACV_SWAP_TRACE_SECONDS", 60);
    if (!options.valid()) {
      std::fputs("[acv-swap-trace] Invalid diagnostic age/duration configuration\n", stderr);
      options.path = nullptr; return;
    }
#if defined(__linux__)
    std::ifstream stat("/proc/self/stat");
    std::string line, field;
    if (std::getline(stat, line)) {
      const auto end = line.rfind(')');
      if (end != std::string::npos) {
        std::istringstream stream(line.substr(end+2));
        for (int i=0; i<=19; ++i) if (!(stream >> field)) return;
        const auto hz = sysconf(_SC_CLK_TCK);
        if (hz <= 0) return;
        try { process_start = std::stoull(field) / double(hz); } catch (...) { return; }
      }
    }
#endif
    if (process_start < 0)
      std::fputs("[acv-swap-trace] Process age unavailable; trace disabled\n", stderr);
  }
  double age() const {
#if defined(__linux__)
    timespec now{};
    if (process_start >= 0 && !clock_gettime(CLOCK_BOOTTIME, &now))
      return now.tv_sec + now.tv_nsec/1e9 - process_start;
#endif
    return -1;
  }
  Recorder<>* begin() {
    if (!options.path || !*options.path) return nullptr;
    if (recorder && recorder->stopped()) return nullptr;
    const auto current = age();
    if (current < options.after_s || !std::isfinite(current)) return nullptr;
    if (!recorder) {
      recorder.reset(new (std::nothrow) Recorder<>(options));
      if (!recorder) {
        std::fputs("[acv-swap-trace] Cannot allocate bounded trace buffer\n", stderr);
        options.path = nullptr; return nullptr;
      }
    }
    return recorder->ready(current) ? recorder.get() : nullptr;
  }
};
inline Runtime& runtime() { static thread_local Runtime instance; return instance; }
inline Recorder<>* begin() { return runtime().begin(); }
inline void finish(Recorder<>* recorder, Clock::time_point arrival, Clock::time_point paced) {
  if (!recorder) return;
  const auto done = Clock::now();  // Capture before serialization at the end of the window.
  recorder->add(ticks(arrival), ticks(paced), ticks(done), runtime().age());
}
}  // namespace acv::swap_trace
