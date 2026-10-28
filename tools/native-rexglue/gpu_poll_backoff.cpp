// Experimental host scheduling for ACV's GPU completion polling.
// Retail bytes: decompilation/evidence/acv/82BAE308-20261027T141204.708311Z
// and 82BAF218-20261027T141231.787259Z; raw LLVM fallback in
// logs/acv-poll-backoff-20261027/. Ghidra function bodies are truncated.
// Generated weak-alias ABI: acv_linux_probe_pch.h; __imp__ is always original.
#include "acv_linux_probe_pch.h"
#include <chrono>
#include <cstdlib>
#include <thread>

REX_EXTERN(__imp__sub_82BAE308);

namespace {
bool BackoffEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("ACV_GPU_POLL_BACKOFF");
    return value && value[0] == '1' && value[1] == '\0';
  }();
  return enabled;
}
}  // namespace

// @retail 0x82BAE308, invoked by the poll loop at 0x82BAF2AC.
// Original evaluates progress, cancellation and timeout; the caller rechecks
// the completion counter after every return. Xenon delay hints at
// 0x82BAE328..344 execute 4 times and are omitted by current codegen.
// This opt-in experiment is a host scheduling backoff, not cycle emulation.
REX_EXTERN(sub_82BAE308) {
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  __imp__sub_82BAE308(ctx, base);
  if (caller == 0x82BAF2B0 && ctx.r3.u32 == 1 && BackoffEnabled()) {
    // Never replace a completion value or skip the original helper. A GPU
    // completion racing this sleep is observed by the caller on return.
    // Host scheduling can overshoot; pacing must be measured before acceptance.
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
}
