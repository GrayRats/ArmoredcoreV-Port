#!/usr/bin/env python3
"""Compile the actual SDK dispatch methods with a concurrent host worker."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root / "tools/rexglue-sdk/src/graphics/command_processor.cpp").read_text()
start = source.index("void CommandProcessor::CallInThread(")
end = source.index("void CommandProcessor::ClearCaches()", start)
methods = source[start:end]
wait_start = source.index("bool CommandProcessor::ExecutePacketType3_WAIT_REG_MEM(")
wait_end = source.index("bool CommandProcessor::ExecutePacketType3_REG_RMW(", wait_start)
wait_method = source[wait_start:wait_end]
assert "ExecuteReadbackFunctions();" in wait_method
assert "ExecutePendingFunctions();" not in wait_method
prelude = r"""
#include <atomic>
#include <chrono>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <iostream>
namespace system_stub {
struct XThread {
  std::thread::id owner;
  static bool IsInThread(XThread* p) { return p && p->owner == std::this_thread::get_id(); }
};
}
struct Event { std::atomic<int> signals{0}; void Set() { ++signals; } };
namespace memory {
struct RingBuffer {
  unsigned index = 0;
  uint32_t values[5] = {3, 0, 1, UINT32_MAX, 0x100};
  template<class T> T ReadAndSwap() { assert(index < 5); return values[index++]; }
};
struct Memory { void* TranslatePhysical(uint32_t) { assert(false); return nullptr; } };
}
namespace xenos { enum class Endian {}; inline uint32_t GpuSwap(uint32_t v, Endian) { return v; } }
namespace rex::thread {
inline void MaybeYield() { std::this_thread::yield(); }
inline void SyncMemory() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void Sleep(std::chrono::milliseconds d) { std::this_thread::sleep_for(d); }
}
#define SCOPE_profile_cpu_f(x) ((void)0)
#define REXCVAR_GET(x) false
constexpr uint32_t XE_GPU_REG_COHER_STATUS_HOST = UINT32_MAX;
struct CommandProcessor {
  void CallInThread(std::function<void()> fn);
  void CallInThreadForReadback(std::function<void()> fn);
  void ExecuteReadbackFunctions();
  bool ExecutePacketType3_WAIT_REG_MEM(memory::RingBuffer*, uint32_t, uint32_t);
  std::atomic<uint32_t> guest_value{0};
  std::atomic<bool> worker_running_{true};
  memory::Memory* memory_ = nullptr;
  uint32_t ReadRegisterValue(uint32_t) { return guest_value.load(); }
  uint32_t ReadPhysicalMemory32(uint32_t) { assert(false); return 0; }
  void MakeCoherent() {}
  void PrepareForWait() {}
  void ReturnFromWait() {}
  bool HasPendingFunctions();
  void ExecutePendingFunctions();
  std::mutex pending_fns_mutex_;
  std::queue<std::function<void()>> pending_fns_;
  std::queue<std::function<void()>> readback_fns_;
  std::shared_ptr<system_stub::XThread> worker_thread_ = std::make_shared<system_stub::XThread>();
  std::shared_ptr<Event> write_ptr_index_event_ = std::make_shared<Event>();
};
"""
test = r"""
int main() {
  CommandProcessor cp;
  constexpr int producers = 8, count = 4000;
  std::vector<std::atomic<int>> seen(producers * count);
  for(auto& n : seen) n.store(0);
  std::vector<int> next(producers, 0); // GPU worker only
  std::atomic<int> completed{0}, nested{0};
  std::atomic<bool> ready{false};
  std::thread worker([&] {
    cp.worker_thread_->owner = std::this_thread::get_id();
    ready.store(true);
    while(completed.load() != producers * count || cp.HasPendingFunctions()) {
      cp.ExecutePendingFunctions(); std::this_thread::yield();
    }
  });
  while(!ready.load()) std::this_thread::yield();
  std::vector<std::thread> senders;
  for(int p=0;p<producers;++p) senders.emplace_back([&,p] {
    for(int i=0;i<count;++i) cp.CallInThread([&,p,i] {
      assert(system_stub::XThread::IsInThread(cp.worker_thread_.get()));
      assert(next[p]++ == i);
      assert(seen[p*count+i].fetch_add(1) == 0);
      cp.CallInThread([&] { ++nested; });
      ++completed;
    });
  });
  for(auto& t : senders) t.join(); worker.join();
  assert(completed == producers*count && nested == producers*count);
  for(auto& n : seen) assert(n == 1);
  assert(!cp.HasPendingFunctions());
  assert(cp.write_ptr_index_event_->signals >= producers*count);
  // Original per-producer FIFO ordering survives multiple producers.
  cp.worker_thread_->owner = std::this_thread::get_id();
  int value = 0;
  cp.CallInThread([&] { ++value; cp.CallInThread([&] { ++value; }); });
  assert(value == 2 && !cp.HasPendingFunctions());
  // Actual SDK WAIT_REG_MEM must service a readback that allows CPU progress.
  // The ordinary queue must remain untouched until this guest packet completes.
  std::atomic<bool> waiting{false};
  int ordinary = 0, readback = 0;
  std::thread stalled_worker([&] {
    cp.worker_thread_->owner = std::this_thread::get_id();
    waiting.store(true);
    memory::RingBuffer packet;
    assert(cp.ExecutePacketType3_WAIT_REG_MEM(&packet, 0, 5));
    assert(readback == 1 && ordinary == 0);
    cp.ExecutePendingFunctions();
    assert(ordinary == 1);
  });
  while(!waiting.load()) std::this_thread::yield();
  cp.CallInThread([&] { ++ordinary; });
  cp.CallInThreadForReadback([&] { ++readback; cp.guest_value.store(1); });
  stalled_worker.join();
  assert(!cp.HasPendingFunctions());
  // Restricted queue itself must also tolerate simultaneous CPU fault requests.
  std::atomic<int> readback_done{0};
  std::atomic<bool> readback_worker_ready{false};
  std::vector<std::atomic<int>> readback_seen(producers * count);
  for(auto& n : readback_seen) n.store(0);
  std::thread readback_worker([&] {
    cp.worker_thread_->owner = std::this_thread::get_id();
    readback_worker_ready.store(true);
    while(readback_done.load() != producers * count || cp.HasPendingFunctions()) {
      cp.ExecuteReadbackFunctions(); std::this_thread::yield();
    }
  });
  while(!readback_worker_ready.load()) std::this_thread::yield();
  senders.clear();
  for(int p=0;p<producers;++p) senders.emplace_back([&,p] {
    for(int i=0;i<count;++i) cp.CallInThreadForReadback([&,p,i] {
      assert(system_stub::XThread::IsInThread(cp.worker_thread_.get()));
      assert(readback_seen[p*count+i].fetch_add(1) == 0);
      ++readback_done;
    });
  });
  for(auto& t : senders) t.join(); readback_worker.join();
  for(auto& n : readback_seen) assert(n == 1);
  assert(!cp.HasPendingFunctions());
  // Shutdown must break even short guest waits without a matching value.
  cp.worker_thread_->owner = std::this_thread::get_id();
  cp.worker_running_.store(false); cp.guest_value.store(0);
  memory::RingBuffer stopped_packet; stopped_packet.values[4] = 0;
  assert(!cp.ExecutePacketType3_WAIT_REG_MEM(&stopped_packet, 0, 5));
  std::cout << "PASS: 32000 cross-thread callbacks and 32000 nested callbacks, exactly once on owner; wake, inline recursion actual WAIT_REG_MEM readback cycle, 32000 readback requests and short-wait shutdown\n";
}
"""
# Only substitute the namespace name to avoid the POSIX system() identifier.
with tempfile.TemporaryDirectory(prefix="acv-dispatch-", dir=root/"tmp") as folder:
    folder = Path(folder)
    cpp = folder/"test.cpp"; binary = folder/"test"
    cpp.write_text(prelude + methods.replace("system::XThread", "system_stub::XThread") + wait_method + test)
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-pthread",
                    "-fsanitize=address,undefined", "-g", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
