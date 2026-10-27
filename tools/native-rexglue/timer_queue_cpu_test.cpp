// Exercise actual shared-library TimerQueue APIs, not a scheduler model.
#include <rex/thread/timer_queue.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <time.h>
using namespace std::chrono_literals;
using namespace rex::thread;
using Clock = TimerQueueWaitItem::clock;
double ProcessCpuSeconds() {
  timespec value{}; assert(!clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value));
  return value.tv_sec + value.tv_nsec / 1e9;
}
struct Signal {
  std::atomic<unsigned> calls{0}; std::mutex mutex; std::condition_variable cv;
  void hit() { std::lock_guard lock(mutex); ++calls; cv.notify_all(); }
  void await(unsigned count) {
    std::unique_lock lock(mutex);
    assert(cv.wait_for(lock, 3s, [&] { return calls.load() >= count; }));
  }
};
int main(int argc, char** argv) {
  const bool require_low = argc == 2 && !std::strcmp(argv[1], "--require-low-cpu");
  auto wall = Clock::now(); const auto cpu = ProcessCpuSeconds();
  std::this_thread::sleep_for(700ms);
  const double elapsed = std::chrono::duration<double>(Clock::now() - wall).count();
  const double idle_cpu = (ProcessCpuSeconds() - cpu) / elapsed * 100;
  // A newly inserted earlier deadline must wake a dispatcher waiting far ahead.
  Signal early; std::atomic<bool> far_fired{false};
  auto far = QueueTimerOnce([&](void*) { far_fired = true; }, nullptr, Clock::now() + 2s).lock();
  assert(far); std::this_thread::sleep_for(10ms);
  const auto due = Clock::now() + 5ms;
  QueueTimerOnce([&](void*) { assert(Clock::now() >= due); early.hit(); }, nullptr, due);
  early.await(1); far->Disarm(); assert(!far_fired);
  // A recurring callback can disarm itself; it must not be requeued again.
  Signal periodic; std::weak_ptr<TimerQueueWaitItem> recurring;
  {
    std::lock_guard lock(periodic.mutex);
    recurring = QueueTimerRecurring([&](void*) {
      std::lock_guard lock(periodic.mutex);
      if (++periodic.calls == 5) { auto item = recurring.lock(); assert(item); item->Disarm(); }
      periodic.cv.notify_all();
    }, nullptr, Clock::now() + 10ms, 3ms);
  }
  periodic.await(5); std::this_thread::sleep_for(20ms); assert(periodic.calls == 5);
  // External cancellation waits for an already executing callback.
  Signal entered; std::mutex held_mutex; std::condition_variable held_cv;
  bool release = false; std::atomic<bool> cancel_done{false};
  auto held = QueueTimerOnce([&](void*) {
    std::unique_lock lock(held_mutex); entered.hit();
    held_cv.wait(lock, [&] { return release; });
  }, nullptr, Clock::now() + 5ms).lock(); assert(held); entered.await(1);
  std::jthread canceller([&] { held->Disarm(); cancel_done = true; });
  std::this_thread::sleep_for(10ms); assert(!cancel_done);
  { std::lock_guard lock(held_mutex); release = true; } held_cv.notify_all();
  canceller.join(); assert(cancel_done);
  // Exercise wraparound/backpressure beyond the 512-entry ring with producers.
  Signal stress; std::vector<std::jthread> producers;
  for (int i = 0; i < 4; ++i) producers.emplace_back([&] {
    for (int j = 0; j < 512; ++j)
      QueueTimerOnce([&](void*) { stress.hit(); }, nullptr, Clock::now() + 5ms);
  });
  producers.clear(); stress.await(2048); assert(stress.calls == 2048);
  // Frequent timer deadlines reset the spin backoff, as in active gameplay.
  Signal ticks; std::weak_ptr<TimerQueueWaitItem> ticker;
  const auto active_start = Clock::now(); const double active_cpu_start = ProcessCpuSeconds();
  {
    std::lock_guard lock(ticks.mutex);
    ticker = QueueTimerRecurring([&](void*) {
      std::lock_guard lock(ticks.mutex);
      if (++ticks.calls == 400) { auto item = ticker.lock(); assert(item); item->Disarm(); }
      ticks.cv.notify_all();
    }, nullptr, Clock::now() + 1ms, 1ms);
  }
  ticks.await(400);
  const double active_wall = std::chrono::duration<double>(Clock::now() - active_start).count();
  const double active_cpu = (ProcessCpuSeconds() - active_cpu_start) / active_wall * 100;
  // Actual deadlines, with conservative scheduler-noise tolerance.
  Signal deadlines; std::vector<double> late; std::mutex late_mutex;
  const auto start = Clock::now() + 10ms;
  for (int i = 0; i < 128; ++i) {
    const auto deadline = start + i * 2ms;
    QueueTimerOnce([&, deadline](void*) {
      const double delay = std::chrono::duration<double, std::milli>(Clock::now() - deadline).count();
      assert(delay >= 0);
      { std::lock_guard lock(late_mutex); late.push_back(delay); }
      deadlines.hit();
    }, nullptr, deadline);
  }
  deadlines.await(128); std::sort(late.begin(), late.end());
  assert(late.size() == 128 && late[121] < 20 && late.back() < 100);
  std::printf("{\"idle_cpu_percent\":%.4f,\"idle_wall_s\":%.3f,\"active_1ms_cpu_percent\":%.4f,\"stress_callbacks\":2048,\"deadline_callbacks\":128,\"deadline_p95_late_ms\":%.4f,\"deadline_max_late_ms\":%.4f,\"self_disarm_calls\":5,\"cancel_waits_for_callback\":true}\n",
              idle_cpu, elapsed, active_cpu, late[121], late.back());
  if (require_low) assert(idle_cpu < 3.0 && active_cpu < 5.0);
}
