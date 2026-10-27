// Diagnostic-only perspective experiment. Disabled without the explicit env var.
// Retail proof: logs/acv-ultrawide-20261024/822B4428.txt, call at 0x822B450C.
// Native override ABI: generated/default/acv_linux_probe_pch.h DEFINE_REX_FUNC;
// weak sub_* alias may be overridden; __imp__sub_* always calls original code.
#include "acv_linux_probe_pch.h"
#include <rex/logging.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>

REX_EXTERN(__imp__sub_82BC8980);
REX_EXTERN(__imp__sub_82BC8CC0);

// Read-only census of the projection helpers (ACV_PROJECTION_CENSUS=1): calls, argument ranges and the
// return address of every call site, logged every 10 s. It never changes guest registers or memory.
namespace {
struct CensusRow {
  uint64_t count = 0;
  double lo[6], hi[6];
};
std::mutex g_census_mutex;
std::map<std::pair<int, uint32_t>, CensusRow> g_census;  // (0 perspective / 1 orthographic, LR)
std::chrono::steady_clock::time_point g_census_next{};

bool CensusEnabled() {
  static const bool on = [] { const char* v = std::getenv("ACV_PROJECTION_CENSUS"); return v && *v == '1'; }();
  return on;
}

void CensusRecord(int kind, uint32_t lr, const double* args, int n) {
  std::lock_guard<std::mutex> lock(g_census_mutex);
  auto [it, inserted] = g_census.try_emplace({kind, lr});
  CensusRow& row = it->second;
  for (int i = 0; i < n; ++i) {
    row.lo[i] = inserted || row.count == 0 ? args[i] : std::min(row.lo[i], args[i]);
    row.hi[i] = inserted || row.count == 0 ? args[i] : std::max(row.hi[i], args[i]);
  }
  ++row.count;
  const auto now = std::chrono::steady_clock::now();
  if (g_census_next == std::chrono::steady_clock::time_point{}) g_census_next = now + std::chrono::seconds(10);
  if (now < g_census_next) return;
  g_census_next = now + std::chrono::seconds(10);
  for (const auto& [key, r] : g_census) {
    if (key.first == 0) {
      REXLOG_INFO("[acv-census] perspective lr={:08X} n={} fov=[{:.4f},{:.4f}] aspect=[{:.4f},{:.4f}] near=[{:.3f},{:.3f}] far=[{:.1f},{:.1f}]",
                  key.second, r.count, r.lo[0], r.hi[0], r.lo[1], r.hi[1], r.lo[2], r.hi[2], r.lo[3], r.hi[3]);
    } else {
      REXLOG_INFO("[acv-census] ortho lr={:08X} n={} left=[{:.1f},{:.1f}] right=[{:.1f},{:.1f}] bottom=[{:.1f},{:.1f}] top=[{:.1f},{:.1f}] near=[{:.2f},{:.2f}] far=[{:.1f},{:.1f}]",
                  key.second, r.count, r.lo[0], r.hi[0], r.lo[1], r.hi[1], r.lo[2], r.hi[2], r.lo[3], r.hi[3],
                  r.lo[4], r.hi[4], r.lo[5], r.hi[5]);
    }
  }
}
}  // namespace

// @retail 0x82BC8CC0 orthographic projection (left, right, bottom, top, near, far in f1..f6); census only.
static double ExperimentalHudAspect() {
  static const double value = [] {
    const char* text = std::getenv("ACV_EXPERIMENTAL_HUD_ASPECT");
    if (!text || !*text) return 0.0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(parsed) || parsed < 1.0 || parsed > 4.0) return 0.0;
    REXLOG_WARN("[acv-aspect-experiment] HUD centring for aspect={}; ortho 0x822B5368 only", parsed);
    return parsed;
  }();
  return value;
}

REX_EXTERN(sub_82BC8CC0) {
  // The HUD overlay is a 1280x720 pixel-space ortho (left 0, right 1280, bottom -720, top 0). When the scene is
  // widened the 1280-px render target is stretched to the wide output, so the HUD must occupy only the central
  // 16:9 part of it: widen left/right symmetrically by aspect/(16/9). Full-screen post passes are other sites.
  const double hud = ExperimentalHudAspect();
  if (hud != 0.0 && ctx.lr == 0x822B5368 && ctx.f1.f64 == 0.0 && ctx.f2.f64 == 1280.0 &&
      ctx.f3.f64 == -720.0 && ctx.f4.f64 == 0.0) {
    const double left = ctx.f1.f64, right = ctx.f2.f64, bottom = ctx.f3.f64, top = ctx.f4.f64;
    if (hud >= 16.0 / 9.0) {  // wider: extend left/right so the HUD keeps a centred 16:9 area
      const double extra = 1280.0 * (hud / (16.0 / 9.0) - 1.0) / 2.0;
      ctx.f1.f64 = -extra;
      ctx.f2.f64 = right + extra;
    } else {  // narrower (16:10, 4:3): extend top/bottom (y runs 0 to -720)
      const double extra = 720.0 * ((16.0 / 9.0) / hud - 1.0) / 2.0;
      ctx.f3.f64 = bottom - extra;
      ctx.f4.f64 = top + extra;
    }
    __imp__sub_82BC8CC0(ctx, base);
    ctx.f1.f64 = left;
    ctx.f2.f64 = right;
    ctx.f3.f64 = bottom;
    ctx.f4.f64 = top;
    return;
  }
  if (CensusEnabled()) {
    const double a[6] = {ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, ctx.f5.f64, ctx.f6.f64};
    CensusRecord(1, uint32_t(ctx.lr), a, 6);
  }
  __imp__sub_82BC8CC0(ctx, base);
}

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

// Census of 2026-10-27 (logs/acv-native-linux/20261026T193756597623Z): two perspective call sites carry the 16:9 view
// (FOV 0.52-1.05 rad, far 13000): 0x822B4510 (3981 calls) and the generic builder 0x82300978 (29049 calls, aspects
// 16:9 and 2:1). Only calls whose original aspect is 16:9 are widened, so 2:1 passes and unseen sites stay untouched.
static bool IsSceneCameraSite(uint32_t lr) { return lr == 0x822B4510 || lr == 0x82300978; }

// @retail 0x82BC8980
// Change the 16:9 perspective calls of the two measured sites; other callers pass through.
// This does not claim a complete UltraWide implementation. Do not publish as one.
REX_EXTERN(sub_82BC8980) {
  if (CensusEnabled()) {
    const double a[4] = {ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64};  // fov, aspect, near, far
    CensusRecord(0, uint32_t(ctx.lr), a, 4);
  }
  const double aspect = ExperimentalAspect();
  const double original = ctx.f2.f64;
  const bool selected = aspect != 0.0 && IsSceneCameraSite(uint32_t(ctx.lr)) &&
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
  // The render target is stretched over the output, so only the projection aspect against the SCREEN matters.
  // Wider than 16:9: keep the vertical FOV, widen x (horizontal view grows). Narrower (16:10, 4:3): keep the
  // horizontal view and enlarge the vertical FOV: tan(fov'/2) = tan(fov/2) * (16/9) / aspect.
  const double fov = ctx.f1.f64;
  double new_fov = fov;
  if (aspect < 16.0 / 9.0) {
    new_fov = 2.0 * std::atan(std::tan(fov / 2.0) * (16.0 / 9.0) / aspect);
    if (!(new_fov > 0.0 && new_fov < 3.141592653589793)) {
      __imp__sub_82BC8980(ctx, base);
      return;
    }
  }
  ctx.f1.f64 = new_fov;
  ctx.f2.f64 = aspect;
  __imp__sub_82BC8980(ctx, base);
  // Original helper preserves f1/f2. Restore its inputs after capturing the new output.
  ctx.f1.f64 = fov;
  ctx.f2.f64 = original;
}
