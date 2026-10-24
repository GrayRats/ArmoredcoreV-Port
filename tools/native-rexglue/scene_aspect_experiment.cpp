// Diagnostic-only perspective experiment. Disabled without the explicit env var.
// Retail proof: logs/acv-ultrawide-20261024/822B4428.txt, call at 0x822B450C.
// Native override ABI: generated/default/acv_linux_probe_pch.h DEFINE_REX_FUNC;
// weak sub_* alias may be overridden; __imp__sub_* always calls original code.
#include "acv_linux_probe_pch.h"
#include <rex/logging.h>
#include <atomic>
#include <cmath>
#include <cstdlib>

REX_EXTERN(__imp__sub_82BC8980);

static double ExperimentalAspect() {
  static const double value = [] {
    const char* text = std::getenv("ACV_EXPERIMENTAL_SCENE_ASPECT");
    if (!text || !*text) return 0.0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(parsed) || parsed < 1.0 || parsed > 4.0) {
      REXLOG_WARN("[acv-aspect-experiment] Invalid aspect; experiment disabled");
      return 0.0;
    }
    REXLOG_WARN("[acv-aspect-experiment] Enabled aspect={}; projection only, HUD/culling/output geometry unverified", parsed);
    return parsed;
  }();
  return value;
}

// @retail 0x82BC8980
// Change only the direct perspective call of 0x822B4428; other callers pass through.
// This does not claim a complete UltraWide implementation. Do not publish as one.
REX_EXTERN(sub_82BC8980) {
  const double aspect = ExperimentalAspect();
  const double original = ctx.f2.f64;
  const bool selected = aspect != 0.0 && ctx.lr == 0x822B4510 &&
      std::isfinite(original) && std::abs(original - 16.0 / 9.0) < 1e-5 &&
      std::isfinite(ctx.f1.f64) && ctx.f1.f64 > 0.0 && ctx.f1.f64 < 3.141592653589793;
  if (!selected) {
    __imp__sub_82BC8980(ctx, base);
    return;
  }
  static std::atomic<unsigned> samples{0};
  if (samples.fetch_add(1, std::memory_order_relaxed) < 4) {
    REXLOG_INFO("[acv-aspect-experiment] caller=822B450C old={} new={} fov={} near={} far={}",
                original, aspect, ctx.f1.f64, ctx.f3.f64, ctx.f4.f64);
  }
  ctx.f2.f64 = aspect;
  __imp__sub_82BC8980(ctx, base);
  // Original helper preserves f2. Restore its input after capturing the new output.
  ctx.f2.f64 = original;
}
